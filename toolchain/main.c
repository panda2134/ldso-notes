#include <stdint.h>
#include "utils.h"

// The _start function should be the first function in main.c. It is the entry point of dynamic linker.
noreturn void _start() {
    asm volatile (
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

hidden noplt void __dl_puts_s(char *buf, size_t len) {
    asm volatile (
        "movq $1, %%rax;" /* write */
        "movq $1, %%rdi;" /* stdout */
        "movq %0, %%rsi;"
        "movq %1, %%rdx;"
        "syscall" 
        :
        : "r"(buf), "r"(len)
        : "rax", "rdi", "rsi", "rdx"
    );
}

hidden noplt size_t __dl_strlen(char *buf) {
    size_t j = 0;
    for (; *buf; buf++, j++);
    return j;
}

hidden noplt void __dl_puts(char *buf) {
    __dl_puts_s(buf, __dl_strlen(buf));
}

hidden noplt void __dl_print_int(uint64_t n) {
    char s[25];
    for(int i = 0; i < 25; i++) s[i] = 0;
    s[24] = '\0';
    for (int i = 23; i >= 0 && n != 0; i--, n /= 10) {
        s[i] = (n % 10) + '0';
    }
    char* h = s;
    while(*h == '\0') h++;
    __dl_puts(h);
}

hidden noplt int _start_c(void* sp) {
    size_t argc = *(size_t*)sp;
    char** argv = (void*)((char*)sp + 8);
    for (int i = 0; i < argc; i++) {
        __dl_puts(argv[i]);
        __dl_puts("\n");
    }
    return 0;
}
