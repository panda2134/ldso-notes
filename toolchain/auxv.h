#pragma once
#include <stdint.h>

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_NOTELF 10
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14


typedef struct {
    uint64_t a_type;
    union {
        int64_t a_val;
        void *a_ptr;
        void (*a_fnc)();
    } a_un;
} auxv_t;