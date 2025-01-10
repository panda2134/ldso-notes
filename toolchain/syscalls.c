#include "syscalls.h"

hidden noplt void * __dl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    void *ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movq %2, %%rsi;"
        "movl %3, %%edx;"
        "movl %4, %%r10d;"
        "movl %5, %%r8d;"
        "movq %6, %%r9;"
        "movq $9, %%rax;" // SYS_mmap
        "syscall;"
        "movq %%rax, %0"
        : "=r" (ret)
        : "r" (addr), "r" (len), "r" (prot), "r" (flags), "r" (fd), "r" (off)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt int __dl_munmap(void *addr, size_t len) {
    int ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movq %2, %%rsi;"
        "movq $11, %%rax;" // SYS_munmap
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (addr), "r" (len)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt int __dl_mprotect(void *addr, size_t len, int prot) {
    int ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movq %2, %%rsi;"
        "movl %3, %%edx;"
        "movq $10, %%rax;" // SYS_mprotect
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (addr), "r" (len), "r" (prot)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt int __dl_open(const char *pathname, int flags, int mode) {
    int ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movl %2, %%esi;"
        "movl %3, %%edx;"
        "movq $2, %%rax;" // SYS_open
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (pathname), "r" (flags), "r" (mode)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt int __dl_close(int fd) {
    int ret = 0;
    asm volatile (
        "movl %1, %%edi;"
        "movq $3, %%rax;" // SYS_close
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (fd)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt int __dl_fstat(int fd, struct stat* buf) {
    int ret = 0;
    asm volatile (
        "movl %1, %%edi;"
        "movq %2, %%rsi;"
        "movq $5, %%rax;" // SYS_fstat
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (fd), "r" (buf)
        : "rcx", "r11", "rax"
    );
    return ret;
}