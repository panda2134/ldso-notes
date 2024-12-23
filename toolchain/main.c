#include <sys/syscall.h>
#include <stdint.h>
#define PUTS(buf,len) asm volatile (\
        "movq $1, %%rax;"\
        "movq $1, %%rdi;" /* stdout */\
        "movq %0, %%rsi;"\
        "movq %1, %%rdx;"\
        "syscall" \
        : \
        : "r"(buf), "r"((uint64_t)len)\
        : "rax", "rdi", "rsi", "rdx"\
    )

void _start_c(void* sp) {
    uint64_t argc = *(uint64_t*)sp;
    uint8_t** argv = (void*)((uint8_t*)sp + 8);
    for (int i = 0; i < argc; i++) {
        uint8_t* v = argv[i];
        uint64_t j = 0;
        for (uint8_t* v1 = v; *v1; v1++,j++);
        PUTS(v, j);
        PUTS("\n", 1);
    }
}
