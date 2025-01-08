#include <stdint.h>
#include "utils.h"
#include "auxv.h"
#include "elf.h"

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
    // for (; *envp; envp++) {
    //     __dl_puts(*envp);
    // }

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
        __dl_puts("ERROR: Program header entry size != 56");
        return 127;
    }

    if (!ldso_base) {
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
            __dl_puts("ERROR: cannot decide base address of ld.so");
            return 127;
        }
    }
    // otherwise, ld.so is load by kernel and its base address is already available.
    __dl_stdout_fputs(" ld.so base = "); __dl_print_hex((uint64_t)ldso_base);

    

    return 0;
}
