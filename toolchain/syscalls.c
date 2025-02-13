#include "syscalls.h"
#include <sys/types.h>

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

hidden noplt int64_t __dl_read(uint32_t fd, char *buf, size_t count) {
    int64_t ret = 0;
    asm volatile (
        "movl %1, %%edi;"
        "movq %2, %%rsi;"
        "movq %3, %%rdx;"
        "movq $0, %%rax;" // SYS_read
        "syscall;"
        "movq %%rax, %0"
        : "=r" (ret)
        : "r" (fd), "r" (buf), "r" (count)
        : "rcx", "r11", "rax"
    );
    return ret;
}

hidden noplt uint64_t __dl_lseek(uint32_t fd, uint64_t offset, uint32_t origin) {
    uint64_t ret = 0;
    asm volatile (
        "movl %1, %%edi;"
        "movq %2, %%rsi;"
        "movl %3, %%edx;"
        "movq $0, %%rax;" // SYS_read
        "syscall;"
        "movq %%rax, %0"
        : "=r" (ret)
        : "r" (fd), "r" (offset), "r" (origin)
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

hidden noplt int __dl_stat(const char* path, struct stat* buf) {
    int ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movq %2, %%rsi;"
        "movq $4, %%rax;" // SYS_stat
        "syscall;"
        "movl %%eax, %0"
        : "=r" (ret)
        : "r" (path), "r" (buf)
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

hidden noplt int64_t __dl_readlink(const char *pathname, char *buf, size_t bufsize) {
    int64_t ret = 0;
    asm volatile (
        "movq %1, %%rdi;"
        "movq %2, %%rsi;"
        "movq %3, %%rdx;"
        "movq $89, %%rax;" // SYS_readlink
        "syscall;"
        "movq %%rax, %0"
        : "=r" (ret)
        : "r" (pathname), "r" (buf), "r" (bufsize)
        : "rcx", "r11", "rax"
    );
    return ret;
}
