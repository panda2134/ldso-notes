#include "utils.h"
#include "malloc.h"
#include "stdbool.h"

hidden noplt void __dl_stdout_fputs_s(const char *buf, size_t len) {
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


hidden noplt void __dl_stdout_fputs(const char *buf) {
    __dl_stdout_fputs_s(buf, __dl_strlen(buf));
}

hidden noplt size_t __dl_strlen(const char *buf) {
    size_t j = 0;
    for (; *buf; buf++, j++);
    return j;
}

hidden noplt void __dl_puts(const char *buf) {
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

hidden noreturn noplt void __dl_exit(uint32_t code) {
    asm volatile (
        "xorq %%rdi, %%rdi;"
        "movl %0, %%edi;"
        "movq $60, %%rax;" // SYS_exit
        "syscall;"
        :
        : "r" (code)
        : "rdi", "rax"
    );
}

hidden noreturn noplt void __dl_die(char *msg) {
    __dl_stdout_fputs("ERROR: ");
    __dl_puts(msg);
    __dl_exit(127);
}

hidden noplt void * __dl_memset(void* s, int c, size_t n) {
    char* s1 = s;
    for (; n; n--, s1++) *s1 = 0;
    return s;
}

hidden noplt void * __dl_memcpy(void* dest, const void* src, size_t n) {
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) { d[i] = s[i]; }
    return dest;
}

hidden noplt int __dl_strcmp(const char *a, const char *b) {
    for (; *a && *a == *b; a++, b++);
    return *a - *b;
}

hidden noplt int __dl_strncmp(const char *a, const char *b, size_t n) {
    for (; n > 0 && *a && *a == *b; a++, b++, n--);
    return n ? (*a - *b) : 0;
}

hidden noplt SLNode* __dl_parse_comma_list(const char *s) {
    SLNode *ret = 0;
    SLNode **tail = &ret;
    while (true) {
        const char *t = s;
        while (*t != '\0' && *t != ':') t++;

        char *new_str = __dl_malloc(t-s+1);
        for (size_t i = 0; i < t - s; i++) new_str[i] = s[i];
        new_str[t-s] = '\0';
        SL_APPEND(new_str, tail);

        if (*t == ':') s = t + 1;
        else break;
    }
    return ret;
}

// Taken from https://flapenguin.me/elf-dt-gnu-hash
hidden noplt uint32_t __dl_gnu_hash(const char* name) {
    uint32_t h = 5381;

    for (; *name; name++) {
        h = (h << 5) + h + *name;
    }

    return h;
}

// Taken from https://maskray.me/blog/2022-08-21-glibc-and-dt-gnu-hash
hidden noplt uint32_t __dl_gnu_hash_get_num_syms(uint32_t *hashtab) {
  uint32_t nbuckets = hashtab[0];
  uint32_t *buckets = hashtab + 4 + hashtab[2]*(sizeof(size_t)/4);
  uint32_t idx = 0;
  for (uint32_t i = nbuckets; i--; )
    if ((idx = buckets[i]) != 0)
      break;
  if (idx != 0) {
    uint32_t *chain = buckets + nbuckets - hashtab[1];
    while (chain[idx++] % 2 == 0);
  }
  return idx;
}