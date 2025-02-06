#include "libtester.h"

extern int a;

extern int putchar(int c);

int main() {
    putchar('H');
    putchar('i');
    print_hex(a);
    print_hex(foo());
    a = 2;
    print_hex(a);
    print_hex(foo());
}
