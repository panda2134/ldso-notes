#pragma once
#include "utils.h"
#include "syscalls.h"
#include "malloc.h"
#include <stdbool.h>
#include <elf.h>
#include <sys/types.h>


typedef struct dl_elf_info {
    dev_t dev;
    ino_t ino;
    uint16_t elf_type;
    size_t base;
    Elf64_Phdr* ph;
    int64_t phnum;

    // below are extras; they can be loaded by calling __dl_loadelf_extras()
    Elf64_Dyn* dyn;
    SLNode* dep_names;
    char *str_table;
    Elf64_Sym *sym_table;
    char *runpath;

    struct dl_elf_info *dep_elfs;
    struct dl_elf_info *next;
} DlElfInfo;

#define DL_APPEND_NODE(node, tail_ptr) do {\
            node->next = 0; \
            *tail_ptr = node; \
            tail_ptr = &(node->next); \
} while (0)

hidden noplt bool __dl_checkelf(Elf64_Ehdr *ehdr);
hidden noplt DlElfInfo * __dl_loadelf(const char* path);
hidden noplt bool __dl_loadelf_extras(DlElfInfo *ret);
hidden noplt void __dl_parse_dyn(Elf64_Dyn *dyn, uint64_t dynv[]);