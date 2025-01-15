#include "libtester.h"

extern int a;

void _start() {
    print_hex(a);
    print_hex(foo());
    a = 2;
    print_hex(a);
    print_hex(foo());
}