#pragma once

#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>

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

#define SL_APPEND(val, tail_ptr) do {\
            SLNode *node = __dl_malloc(sizeof(SLNode)); \
            node->s = (val); \
            node->next = 0; \
            *tail_ptr = node; \
            tail_ptr = &(node->next); \
} while (0)

// Forward tail_ptr to tail of linked list. tail_ptr must point to valid node.
#define SL_GOTO_END(tail_ptr) while ((*(tail_ptr))->next) { tail_ptr = &((*(tail_ptr))->next); }

hidden noplt void __dl_stdout_fputs(const char *buf);
hidden noplt void __dl_stdout_fputs_s(const char *buf, size_t len);
hidden noplt size_t __dl_strlen(const char *buf);
hidden noplt int __dl_strcmp(const char *a, const char *b);
hidden noplt int __dl_strncmp(const char *a, const char *b, size_t len);
hidden noplt void __dl_puts(const char* buf);
hidden noplt void __dl_print_int(int64_t n);
hidden noplt void __dl_print_hex(uint64_t n);
hidden noplt void * __dl_memset(void* s, int c, size_t n);
hidden noplt void *__dl_memcpy(void* dest, const void* src, size_t n);
hidden noplt SLNode* __dl_parse_comma_list(const char *s);
hidden noplt uint32_t __dl_gnu_hash(const char* name);
hidden noplt uint32_t __dl_gnu_hash_get_num_syms(uint32_t *hashtab);

hidden noreturn noplt void __dl_die(char *msg);