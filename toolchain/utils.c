#include "utils.h"

hidden noplt void __dl_stdout_fputs_s(char *buf, size_t len) {
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


hidden noplt void __dl_stdout_fputs(char *buf) {
    __dl_stdout_fputs_s(buf, __dl_strlen(buf));
}

hidden noplt size_t __dl_strlen(char *buf) {
    size_t j = 0;
    for (; *buf; buf++, j++);
    return j;
}

hidden noplt void __dl_puts(char *buf) {
    __dl_stdout_fputs_s(buf, __dl_strlen(buf));
    __dl_stdout_fputs_s("\n", 1);
}

hidden noplt void __dl_print_int(int64_t n) {
    if (n < 0) {
        n = -n;
        __dl_stdout_fputs_s("-", 1);
    }

    if (n == 0) {
        __dl_puts("0");
    } else {
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
}

hidden noplt void __dl_print_hex(uint64_t n) {
    char *t = "0123456789abcdef";
    char s[19];
    s[0] = '0'; s[1] = 'x';
    s[18] = '\0';
    for (int i = 17; i > 1; i--) {
        s[i] = t[n & 0xf];
        n >>= 4;
    }
    __dl_puts(s);
}