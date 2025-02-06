#include "main.h"
#include "utils.h"
#include "syscall.h"
#include "walkdyn.h"
#include "global.h"
#include <elf.h>
#include <linux/limits.h>

static uint64_t dynv[DT_NUM];
static DlElfInfo *exec = 0;
static int ret_code = 0;

// The _start function should be the first function in main.c. It is the entry point of dynamic linker.
// Do not move it around, as its address is used for detecting self-relocation.
noreturn void _start() {
    asm volatile (
        "mov $0, %%rbp;"
        "mov %%rsp, %%rdi;"
        "add $0x8, %%rdi;"
        "call _start_c;"   // Passes control to _start_c
        :
        :
        : "rdi", "rax"
    );
    run_init();
    __dl_puts("Let's take over the world!");
    asm volatile ("mov $0, %%rbp;"
        "add $0x8, %%rsp;"
        "jmp *%0;"
        :: "m"(exec->entry));
    /*
    I've fixed the stack layout and symbol search of ld.so global data.
    However, __init_ssp depends on pthread_self. We will have to work around this.
    */
}

hidden noplt void _start_c(void* sp) {
    __dl_stdout_fputs("Stack pointer = "); __dl_print_hex((uint64_t)sp);
    size_t argc = *(size_t*)sp;
    char** argv = (void*)((char*)sp + 8);
    __dl_print_int(argc);
    __dl_stdout_fputs("Arguments: ");
    for (int i = 0; i < argc; i++) {
        __dl_stdout_fputs(argv[i]);
        __dl_stdout_fputs(" ");
    }
    __dl_puts("");

    // __dl_puts("Environs:");
    char** envp = argv + (argc + 1);
    EnvLdConfig conf;
    conf.lib_path = conf.preload = 0;
    __environ = envp;
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
    uint64_t at_entry = 0;
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
            case AT_ENTRY:
                at_entry = auxv->a_un.a_val;
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

    // If we need to load the target executable on our own, we'll have to move ld.so around if necessary.
    // Otherwise the kernel ELF loader would have done that for us.
    if (direct_invoke) {
        if (argc != 2) {
            __dl_die("Usage: ./ld.my.so execname");
        }
        exec = __dl_loadelf(argv[1]);
    } else {
        exec = __dl_malloc(sizeof(DlElfInfo));

        exec->elf_type = ET_EXEC; // default
        exec->ph = at_phdr;
        exec->phnum = at_phnum;

        // Fill in the dev/ino
        struct stat statbuf;
        if (__dl_stat(at_execfn, &statbuf) != 0) {
            __dl_stdout_fputs("stat() failed: "); __dl_puts(at_execfn);
            __dl_die("cannot find executable");
        }
        exec->dev = statbuf.st_dev; exec->ino = statbuf.st_ino;
        __dl_stdout_fputs("EXECFN (dev,ino): ");
        __dl_print_int(exec->dev);
        __dl_print_int(exec->ino);

        // Find out base address using entry 0 in program header (segment) table.
        exec->base = 0;
        for (int64_t i = 0; i < exec->phnum; i++) {
            Elf64_Phdr *e = exec->ph + i;
            if (e->p_type == PT_PHDR) {
                exec->base = (uint64_t)exec->ph - (uint64_t)e->p_vaddr;
                break;
            }
        }
        exec->entry = (void*)(exec->base + at_entry);
        bool r = __dl_loadelf_extras(exec);
        if (!r) __dl_die("loadelf_extra failed");
    }
    // Then, we go through relocation tables. The following code may be reused for loading shared objects.
    Elf64_Dyn *dyn = exec->dyn;
    __dl_stdout_fputs(" exec base = "); __dl_print_hex(exec->base);

    // Parse the dynamic section to get the relocation tables and symbol tables.
    // To perform the relocation, we need to learn about symbol names. Thus symbol tables are needed.
    // Ref: https://refspecs.linuxbase.org/elf/gabi4+/ch5.dynamic.html#dynamic_section
    __dl_parse_dyn(dyn, dynv);
    char *str_table = exec->str_table;
    void* gnu_hashtab = 0;
    for (Elf64_Dyn *p = dyn; p->d_tag; p++) {
        if (p->d_tag == DT_GNU_HASH) {
            gnu_hashtab = (void*)(exec->base + p->d_un.d_ptr);
            break;
        }
    }
    if (dynv[DT_FLAGS] & DF_TEXTREL) {
        __dl_die("text relocation is unsupported");
    }
    uint32_t symbol_num = __dl_gnu_hash_get_num_syms(gnu_hashtab);
    // show symbols defined
    Elf64_Sym *sym = exec->sym_table, *sym_table = exec->sym_table;
    // for (uint32_t i = symbol_num; i; i--, sym++) {
    //     __dl_stdout_fputs("==> Symbol name: ");
    //     if (sym->st_name) {
    //         __dl_puts(str_table + sym->st_name);
    //     } else {
    //         __dl_puts("[NO NAME]");
    //     }
    //     __dl_stdout_fputs("    Symbol value: ");
    //     __dl_print_hex(sym->st_value);
    //     __dl_stdout_fputs("    Symbol size: ");
    //     __dl_print_int(sym->st_size);
    // }
    // TODO: parse the GNU_HASH hashtable for symbols

    // multiple DT_NEEDED entries may exist
    for (SLNode *node = exec->dep_names; node != 0; node = node->next) {
        __dl_stdout_fputs("==> Dependency: ");
        __dl_puts(node->s);
    }

    __dl_stdout_fputs("==> DT_RUNPATH: ");
    __dl_puts(exec->runpath);

    void *rel_table = (void*) (exec->base + dynv[DT_REL]);
    uint64_t rel_cnt = dynv[DT_RELSZ] / sizeof(Elf64_Rel);
    void *rela_table = (void*) (exec->base + dynv[DT_RELA]);
    uint64_t rela_cnt = dynv[DT_RELASZ] / sizeof(Elf64_Rela);
    uint64_t plt_reloc_type = dynv[DT_PLTREL];
    uint64_t plt_reloc_size = dynv[DT_PLTRELSZ];
    void * plt_reloc_table = (void*) (exec->base + dynv[DT_JMPREL]);

    DlInfoHTNode ** dlinfo_ht = __dl_malloc(sizeof(DlInfoHTNode*) * DLINFO_HT_LEN);
    DlRecResult res = __dl_recursive_load_all(exec, &conf, dlinfo_ht);
    __dl_puts("Recursive loading done.");

    for (DlFileInfo *elf_file = res.topo_list; elf_file; elf_file = elf_file->next) {
        DlElfInfo *elf = __dl_ht_lookup(dlinfo_ht, DLINFO_HT_LEN, elf_file->dev, elf_file->ino);
        __dl_relocate(elf, dlinfo_ht, res.preload_list, exec);
    }
    __dl_puts("Relocation done.");

    __dl_stdout_fputs("Stack pointer = "); __dl_print_hex((uint64_t)sp);
}

hidden noplt void run_init() {
    void (*fn)() = (void*) dynv[DT_INIT];
    if (fn != 0) fn();
    for (uint64_t i = 0; i < dynv[DT_INIT_ARRAYSZ]; i += sizeof(void*)) {
        uint64_t init_addr = *(uint64_t*)(dynv[DT_INIT_ARRAY] + i);
        if (init_addr != 0 && init_addr != (uint64_t)-1) {
            fn = (void*) init_addr;
            fn();
        }
    }
}

hidden noplt void run_fini() {
    void (*fn)() = (void*) dynv[DT_FINI];
    for (uint64_t i = 0; i < dynv[DT_FINI_ARRAYSZ]; i += sizeof(void*)) {
        uint64_t fini_addr = *(uint64_t*)(dynv[DT_FINI_ARRAY] + dynv[DT_FINI_ARRAYSZ] - i - sizeof(void*));

        if (fini_addr != 0 && fini_addr != (uint64_t)-1) {
            fn = (void*) fini_addr;
            fn();
        }
    }
    fn = (void*) dynv[DT_FINI];
    if (fn != 0) fn();
}
