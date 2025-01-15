#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf.h>

#define hidden __attribute__((hidden))
#define noplt __attribute__((noplt))
#define noreturn __attribute__((noreturn))

typedef struct env_ld_config {
    const char* preload;
    const char* lib_path;
} EnvLdConfig;

typedef struct str_list_node {
    const char* s;
    struct str_list_node* next;
} SLNode;