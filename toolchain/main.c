#include "main.h"
#include "walkdyn.h"
#include <linux/limits.h>

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
    EnvLdConfig conf;
    conf.lib_path = conf.preload = 0;
    for (; *envp; envp++) {
        // Jump through all environs
        // We need to look for LD_PRELOAD / LD_LIBRARY_PATH
        const char* s = *envp;
        const char libpath_prefix[] = "LD_LIBRARY_PATH=";
        const char preload_prefix[] = "LD_PRELOAD=";
        const size_t libpath_prefix_len = (sizeof(libpath_prefix) / sizeof(char)) - 1;
        const size_t preload_prefix_len = (sizeof(preload_prefix) / sizeof(char)) - 1;
        if (s[0] != 'L' || s[1] != 'D' || s[2] != '_') continue; // must start with "LD_"
        if (__dl_strncmp(s, libpath_prefix, libpath_prefix_len) == 0) {
            conf.lib_path = s + libpath_prefix_len;
        } else if (__dl_strncmp(s, preload_prefix, preload_prefix_len) == 0) {
            conf.preload = s + preload_prefix_len;
        }
    }
    if (conf.lib_path) { __dl_stdout_fputs("lib_path = "); __dl_puts(conf.lib_path); }
    if (conf.preload) { __dl_stdout_fputs("preload = "); __dl_puts(conf.preload); }

    __dl_puts("Aux:");
    auxv_t* auxv = (void*)(envp + 1);
    int64_t at_phnum = 0;
    char *at_execfn = 0;
    void *at_phdr = 0, *at_ldso_base = 0;
    for (; auxv->a_type != AT_NULL; auxv++) {
        switch (auxv->a_type) {
            case AT_EXECFN:
                at_execfn = (void*) auxv->a_un.a_val;
                break;
            case AT_PHDR:
                at_phdr = auxv->a_un.a_ptr;
                break;
            case AT_BASE:
                at_ldso_base = auxv->a_un.a_ptr;
                break;
            case AT_PHNUM:
                at_phnum = auxv->a_un.a_val;
                break;
        }
    }
    __dl_stdout_fputs(" AT_EXECFN:");
    __dl_puts(at_execfn);

    __dl_stdout_fputs(" AT_PHDR:");
    __dl_print_hex((uint64_t) at_phdr);

    __dl_stdout_fputs(" AT_PHNUM:");
    __dl_print_int(at_phnum);
    
    __dl_stdout_fputs(" AT_BASE:");
    __dl_print_hex((uint64_t) at_ldso_base);
    __dl_puts("END");

    bool direct_invoke = false;

    if (!at_ldso_base) {
        direct_invoke = true;
        // ld.so is invoked directly, and we need to find its base address.
        // We might need to move it later in case the target executable is non-PIE (and we'll have to give way).
        __dl_puts("==> ld.my.so is invoked directly from command line");
        // Parse program headers to get the base address.
        // Look for the only LOAD segment that is executable. It contains solely .text section.
        for (int64_t i = 0; i < at_phnum; i++) {
            Elf64_Phdr *e = (Elf64_Phdr*)at_phdr + i;
            if (e->p_type == PT_LOAD && (e->p_flags & PF_X)){
                at_ldso_base = (void*)(((uint64_t)_start) - e->p_vaddr); // Calculate the base vaddr where ld.so is loaded.
            }
        }
        if (!at_ldso_base) {
            __dl_die("cannot decide base address of ld.so");
        }
    }
    // otherwise, ld.so is load by kernel and its base address is already available.
    __dl_stdout_fputs(" ld.so base = "); __dl_print_hex((uint64_t)at_ldso_base);

    DlElfInfo *exec_info = 0;
    
    // If we need to load the target executable on our own, we'll have to move ld.so around if necessary.
    // Otherwise the kernel ELF loader would have done that for us.
    if (direct_invoke) {
        if (argc != 2) {
            __dl_die("Usage: ./ld.my.so execname");
        }
        exec_info = __dl_loadelf(argv[1]);
    } else {
        exec_info = __dl_malloc(sizeof(DlElfInfo));

        exec_info->elf_type = ET_EXEC; // default
        exec_info->ph = at_phdr;
        exec_info->phnum = at_phnum;

        // Fill in the dev/ino
        struct stat statbuf;
        if (__dl_stat(at_execfn, &statbuf) != 0) {
            __dl_stdout_fputs("stat() failed: "); __dl_puts(at_execfn);
            return 0;
        }
        exec_info->dev = statbuf.st_dev; exec_info->ino = statbuf.st_ino;
        __dl_stdout_fputs("EXECFN (dev,ino): ");
        __dl_print_int(exec_info->dev);
        __dl_print_int(exec_info->ino);
        
        // Find out base address using entry 0 in program header (segment) table.
        for (int64_t i = 0; i < exec_info->phnum; i++) {
            Elf64_Phdr *e = exec_info->ph + i;
            if (e->p_type == PT_PHDR) {
                exec_info->base = (uint64_t)exec_info->ph - (uint64_t)e->p_vaddr;
                break;
            }
        }
        bool r = __dl_loadelf_extras(exec_info);
        if (!r) __dl_die("loadelf_extra failed");
    }
    // Then, we go through relocation tables. The following code may be reused for loading shared objects.
    Elf64_Dyn *dyn = exec_info->dyn;
    __dl_stdout_fputs(" exec base = "); __dl_print_hex(exec_info->base);

    // Parse the dynamic section to get the relocation tables and symbol tables.
    // To perform the relocation, we need to learn about symbol names. Thus symbol tables are needed.
    // Ref: https://refspecs.linuxbase.org/elf/gabi4+/ch5.dynamic.html#dynamic_section
    uint64_t dynv[DT_NUM];
    __dl_parse_dyn(dyn, dynv);
    char *str_table = exec_info->str_table;
    void* hashtab = 0;
    for (Elf64_Dyn *p = dyn; p->d_tag; p++) {
        if (p->d_tag == DT_GNU_HASH) {
            hashtab = (void*)(exec_info->base + p->d_un.d_ptr);
            break;
        }
    }
    if (dynv[DT_FLAGS] & DF_TEXTREL) {
        __dl_puts("ERROR: text relocation is unsupported");
        return 1;
    }
    uint32_t symbol_num = __dl_gnu_hash_get_num_syms(hashtab);
    // show symbols defined
    Elf64_Sym *sym = exec_info->sym_table, *sym_table = exec_info->sym_table;
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
    for (SLNode *node = exec_info->dep_names; node != 0; node = node->next) {
        __dl_stdout_fputs("==> Dependency: ");
        __dl_puts(node->s);
    }

    __dl_stdout_fputs("==> DT_RUNPATH: ");
    __dl_puts(exec_info->runpath);

    void *rel_table = (void*) (exec_info->base + dynv[DT_REL]);
    uint64_t rel_cnt = dynv[DT_RELSZ] / sizeof(Elf64_Rel);
    void *rela_table = (void*) (exec_info->base + dynv[DT_RELA]);
    uint64_t rela_cnt = dynv[DT_RELASZ] / sizeof(Elf64_Rela);
    uint64_t plt_reloc_type = dynv[DT_PLTREL];
    uint64_t plt_reloc_size = dynv[DT_PLTRELSZ];
    void * plt_reloc_table = (void*) (exec_info->base + dynv[DT_JMPREL]);

    DlElfInfo *topo_list = __dl_recursive_load_all(exec_info, &conf);
    

    // // read and parse RELA / REL.
    // __dl_puts("Parsing REL/RELA relocations for GOT...");
    // __dl_print_rela_table(rela_cnt, rela_table, sym_table, str_table);
    // __dl_print_rel_table(rel_cnt, rel_table, sym_table, str_table);

    // if (plt_reloc_type != DT_REL && plt_reloc_type != DT_RELA) __dl_die("unsupported PLT relocation type");
    // __dl_stdout_fputs("PLT Relocations will be eagerly bound; Type = ");
    // __dl_puts(plt_reloc_type == DT_RELA ? "RELA" : "REL");
    // uint64_t plt_reloc_cnt = plt_reloc_size / (plt_reloc_type == DT_RELA ? sizeof(Elf64_Rela) : sizeof(Elf64_Rel));
    // if (plt_reloc_type == DT_REL) __dl_print_rel_table(plt_reloc_cnt, plt_reloc_table, sym_table, str_table);
    // else __dl_print_rela_table(plt_reloc_cnt, plt_reloc_table, sym_table, str_table);

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
