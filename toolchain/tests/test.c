#include "libtester.h"

extern int a;

void _start() {
    a = 2;
    foo();
}