#pragma once

#include "syscalls.h"
#include "malloc.h"
#include "defs.h"
#include "tls.h"
#include <stdbool.h>
#include <elf.h>
#include <sys/types.h>


typedef struct dl_file_info {
    dev_t dev;
    ino_t ino;
    struct dl_file_info *next;
} DlFileInfo;

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
    void* gnu_hash_table;
    const char *load_path;
    struct tls_module tls; /* TLS initial image for this DSO */

    uint64_t shoff;		/* Section header table offset */
    uint16_t shentsize;	/* Section header table entry size */
    uint16_t shnum;		/* Section header table entry count */
    uint16_t shstrndx;	/* Section header string table index */

    DlFileInfo *deps;
    bool relocated;
    int (*entry)();
} DlElfInfo;

#define DL_FILE_APPEND_NODE(dev_v, ino_v, tail_ptr) do {\
            DlFileInfo *__node = __dl_malloc(sizeof(DlFileInfo));\
            __node->dev = dev_v; __node->ino = ino_v;\
            __node->next = 0; \
            *tail_ptr = __node; \
            tail_ptr = &(__node->next); \
} while (0)

hidden noplt bool __dl_checkelf(Elf64_Ehdr *ehdr);
hidden noplt DlElfInfo * __dl_loadelf(const char* path);
hidden noplt bool __dl_loadelf_extras(DlElfInfo *ret);
hidden noplt void __dl_parse_dyn(Elf64_Dyn *dyn, uint64_t dynv[]);
