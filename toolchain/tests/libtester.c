typedef unsigned long long size_t;
typedef size_t uint64_t;

int a = 1;
int foo() {
    return a;
}

void stdout_fputs_s(const char *buf, size_t len) {
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

size_t _strlen(const char *buf) {
    size_t j = 0;
    for (; *buf; buf++, j++);
    return j;
}


void stdout_fputs(const char *buf) {
    stdout_fputs_s(buf, _strlen(buf));
}

void _puts(const char *buf) {
    stdout_fputs_s(buf, _strlen(buf));
    stdout_fputs_s("\n", 1);
}
void print_hex(uint64_t n) {
    char *t = "0123456789abcdef";
    char s[19];
    s[0] = '0'; s[1] = 'x';
    s[18] = '\0';
    for (int i = 17; i > 1; i--) {
        s[i] = t[n & 0xf];
        n >>= 4;
    }
    _puts(s);
}