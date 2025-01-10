#include "main.h"

// The _start function should be the first function in main.c. It is the entry point of dynamic linker.
// Do not move it around, as its address is used for detecting self-relocation.
noreturn void _start() {
    asm volatile (
        "mov $0, %%rbp;"
        "mov %%rsp, %%rdi;"
        "add $0x8, %%rdi;"
        "call _start_c;"   // Passes control to _start_c
        "xorq %%rdi, %%rdi;"
        "movl %%eax, %%edi;"
        "movq $60, %%rax;" // SYS_exit
        "syscall;"
        :
        :
        : "rdi", "rax"
    );
}

hidden noplt int _start_c(void* sp) {
    size_t argc = *(size_t*)sp;
    char** argv = (void*)((char*)sp + 8);
    __dl_stdout_fputs("Arguments:");
    for (int i = 0; i < argc; i++) {
        __dl_stdout_fputs(argv[i]);
        __dl_stdout_fputs(" ");
    }
    __dl_puts("");

    // __dl_puts("Environs:");
    char** envp = argv + (argc + 1);
    for (; *envp; envp++) {
        // Jump through all environs
        // do nothing
    }

    __dl_puts("Aux:");
    auxv_t* auxv = (void*)(envp + 1);
    int64_t execfd_val = 0, phent = 0, phnum = 0;
    void *phdr_val = 0, *ldso_base = 0;
    for (; auxv->a_type != AT_NULL; auxv++) {
        switch (auxv->a_type) {
            case AT_EXECFD:
                execfd_val = auxv->a_un.a_val;
                break;
            case AT_PHDR:
                phdr_val = auxv->a_un.a_ptr;
                break;
            case AT_BASE:
                ldso_base = auxv->a_un.a_ptr;
                break;
            case AT_PHENT:
                phent = auxv->a_un.a_val;
                break;
            case AT_PHNUM:
                phnum = auxv->a_un.a_val;
                break;
        }
    }
    __dl_stdout_fputs(" AT_EXECFD:");
    __dl_print_hex(execfd_val);

    __dl_stdout_fputs(" AT_PHDR:");
    __dl_print_hex((uint64_t) phdr_val);

    __dl_stdout_fputs(" AT_PHENT:");
    __dl_print_int(phent);
    __dl_stdout_fputs(" AT_PHNUM:");
    __dl_print_int(phnum);
    
    __dl_stdout_fputs(" AT_BASE:");
    __dl_print_hex((uint64_t) ldso_base);
    __dl_puts("END");

    if (phent != sizeof(Elf64_Phdr)) {
        __dl_die("Program header entry size != 56");
    }

    bool direct_invoke = false;

    if (!ldso_base) {
        direct_invoke = true;
        // ld.so is invoked directly, and we need to find its base address.
        // We might need to move it later in case the target executable is non-PIE (and we'll have to give way).
        __dl_puts("==> ld.my.so is invoked directly from command line");
        // Parse program headers to get the base address.
        // Look for the only LOAD segment that is executable. It contains solely .text section.
        for (int64_t i = 0; i < phnum; i++) {
            Elf64_Phdr *e = (Elf64_Phdr*)phdr_val + i;
            if (e->p_type == PT_LOAD && (e->p_flags & PF_X)){
                ldso_base = (void*)(((uint64_t)_start) - e->p_vaddr); // Calculate the base vaddr where ld.so is loaded.
            }
        }
        if (!ldso_base) {
            __dl_die("cannot decide base address of ld.so");
        }
    }
    // otherwise, ld.so is load by kernel and its base address is already available.
    __dl_stdout_fputs(" ld.so base = "); __dl_print_hex((uint64_t)ldso_base);

    // If we need to load the target executable on our own, we'll have to move ld.so around if necessary.
    // Otherwise the kernel ELF loader would have done that for us.
    if (direct_invoke) {
        if (argc != 2) {
            __dl_die("Usage: ./ld.my.so execname");
        }
        int fd = __dl_open(argv[1], O_RDONLY, 0);
        if (fd < 0) __dl_die("Exec file not found");
        __dl_puts("Loading target exec... ");
        phdr_val = __dl_loadelf(fd, &phnum);
        phent = sizeof(Elf64_Phdr);
    }
    // Then, we go through relocation tables. The following code may be reused for loading shared objects.
    // We first find out the base address of the executable.
    uint64_t exec_base = 0;
    Elf64_Dyn *dyn = 0;

    // Find out base address using entry 0 in program header (segment) table.
    for (int64_t i = 0; i < phnum; i++) {
        Elf64_Phdr *e = (Elf64_Phdr*)phdr_val + i;
        if (e->p_type == PT_PHDR) {
            exec_base = (uint64_t)phdr_val - (uint64_t)e->p_vaddr;
            break;
        }
    }
    __dl_stdout_fputs(" exec base = "); __dl_print_hex(exec_base);
    // Then, find the dynamic section by locating DYNAMIC segment.
    for (int64_t i = 0; i < phnum; i++) {
        Elf64_Phdr *e = (Elf64_Phdr*)phdr_val + i;
        if (e->p_type != PT_DYNAMIC) {
            continue;
        }
        __dl_puts("Dynamic segment found. Reading dynamic section items...");
        dyn = (void*)(exec_base + e->p_vaddr);
    }
    // Parse the dynamic section to get the relocation tables and symbol tables.
    // To perform the relocation, we need to learn about symbol names. Thus symbol tables are needed.
    // Ref: https://refspecs.linuxbase.org/elf/gabi4+/ch5.dynamic.html#dynamic_section
    char *str_table = 0;
    Elf64_Sym *sym_table = 0;
    void* hashtab = 0;
    for (Elf64_Dyn *p = dyn; p->d_tag; p++) {
        switch (p->d_tag) {
            case DT_STRTAB:
                str_table = (void*)(exec_base + p->d_un.d_ptr);
                break;
            case DT_SYMTAB:
                sym_table = (void*)(exec_base + p->d_un.d_ptr);
                break;
            case DT_GNU_HASH:
                hashtab = (void*)(exec_base + p->d_un.d_ptr);
                break;
            case DT_FLAGS:
                if (p->d_un.d_val & DF_TEXTREL) {
                    __dl_puts("ERROR: No text relocation is supported");
                    return 1;
                }
                break;
        }
    }
    uint32_t symbol_num = __dl_gnu_hash_get_num_syms(hashtab);
    // show symbols defined
    Elf64_Sym *sym = sym_table;
    for (uint32_t i = symbol_num; i; i--, sym++) {
        __dl_stdout_fputs("==> Symbol name: ");
        if (sym->st_name) {
            __dl_puts(str_table + sym->st_name);
        } else {
            __dl_puts("[NO NAME]");
        }
        __dl_stdout_fputs("    Symbol value: ");
        __dl_print_hex(sym->st_value);
        __dl_stdout_fputs("    Symbol size: ");
        __dl_print_int(sym->st_size);
    }
    // TODO: parse the GNU_HASH hashtable for symbols

    // multiple DT_NEEDED entries may exist
    void *rel_table = 0, *rela_table = 0, *plt_reloc_table = 0;
    uint64_t rel_cnt = 0, rela_cnt = 0, plt_reloc_type = 0, plt_reloc_size = 0;
    for (Elf64_Dyn *p = dyn; p->d_tag; p++) {
        switch (p->d_tag) {
            case DT_NEEDED:
                __dl_stdout_fputs("==> DT_NEEDED: ");
                __dl_puts(str_table + p->d_un.d_val);
                break;
            case DT_RUNPATH:
                __dl_stdout_fputs("==> DT_RUNPATH: ");
                __dl_puts(str_table + p->d_un.d_val);
                break;
            case DT_REL:
                rel_table = (void*) (exec_base + p->d_un.d_ptr);
                break;
            case DT_RELSZ:
                rel_cnt = p->d_un.d_val / sizeof(Elf64_Rel);
                break;
            case DT_RELA:
                rela_table = (void*) (exec_base + p->d_un.d_ptr);
                break;
            case DT_RELASZ:
                rela_cnt = p->d_un.d_val / sizeof(Elf64_Rela);
                break;
            case DT_PLTREL:
                plt_reloc_type = p->d_un.d_val;
                break;
            case DT_PLTRELSZ:
                plt_reloc_size = p->d_un.d_val;
                break;
            case DT_JMPREL:
                plt_reloc_table = (void*) (exec_base + p->d_un.d_ptr);
                break;
        }
    }
    // read and parse RELA / REL.
    __dl_puts("Parsing REL/RELA relocations for GOT...");
    __dl_print_rela_table(rela_cnt, rela_table, sym_table, str_table);
    __dl_print_rel_table(rel_cnt, rel_table, sym_table, str_table);

    if (plt_reloc_type != DT_REL && plt_reloc_type != DT_RELA) __dl_die("unsupported PLT relocation type");
    __dl_stdout_fputs("PLT Relocations will be eagerly bound; Type = ");
    __dl_puts(plt_reloc_type == DT_RELA ? "RELA" : "REL");
    uint64_t plt_reloc_cnt = plt_reloc_size / (plt_reloc_type == DT_RELA ? sizeof(Elf64_Rela) : sizeof(Elf64_Rel));
    if (plt_reloc_type == DT_REL) __dl_print_rel_table(plt_reloc_cnt, plt_reloc_table, sym_table, str_table);
    else __dl_print_rela_table(plt_reloc_cnt, plt_reloc_table, sym_table, str_table);


    // tests on loading shared object
    {
        int fd = __dl_open("./tests/libtester.so", O_RDONLY, 0);
        if (fd < 0) __dl_die("so file not found");
        __dl_puts("Loading target so... ");
        int64_t phnum;
        void* phdr_val = __dl_loadelf(fd, &phnum);
        Elf64_Phdr *p = phdr_val;
        while (phnum--) __dl_print_hex((p++)->p_type);
    }
    return 0;
}

void __dl_print_rel_table(uint64_t rel_cnt, void *rel_table, Elf64_Sym *sym_table, char *str_table)
{
    __dl_stdout_fputs("REL relocation entries: count = ");
    __dl_print_int(rel_cnt);
    if (rel_table != 0)
    {
        uint64_t r = rel_cnt;
        for (Elf64_Rel *entry = rel_table; r; entry++, r--)
        {
            __dl_stdout_fputs("==> Offset: ");
            __dl_print_hex(entry->r_offset);
            uint64_t reloc_sym = ELF64_R_SYM(entry->r_info), reloc_type = ELF64_R_TYPE(entry->r_info);
            __dl_stdout_fputs("==> Sym: ");
            __dl_puts(str_table + (sym_table + reloc_sym)->st_name);
            __dl_stdout_fputs("==> Type: ");
            __dl_print_hex(reloc_type);
        }
    }
}

void __dl_print_rela_table(uint64_t rela_cnt, void *rela_table, Elf64_Sym *sym_table, char *str_table)
{
    __dl_stdout_fputs("RELA relocation entries: count = ");
    __dl_print_int(rela_cnt);
    if (rela_table != 0)
    {
        uint64_t r = rela_cnt;
        for (Elf64_Rela *entry = rela_table; r; entry++, r--)
        {
            __dl_stdout_fputs("==> Offset: ");
            __dl_print_hex(entry->r_offset);
            __dl_stdout_fputs("==> Addend: ");
            __dl_print_hex(entry->r_addend);
            uint64_t reloc_sym = ELF64_R_SYM(entry->r_info), reloc_type = ELF64_R_TYPE(entry->r_info);
            __dl_stdout_fputs("==> Sym: ");
            __dl_puts(str_table + (sym_table + reloc_sym)->st_name);
            __dl_stdout_fputs("==> Type: ");
            __dl_print_hex(reloc_type);
        }
    }

}
