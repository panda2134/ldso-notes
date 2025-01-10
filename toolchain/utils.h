#pragma once

#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#define hidden __attribute__((hidden))
#define noplt __attribute__((noplt))
#define noreturn __attribute__((noreturn))

hidden noplt void __dl_stdout_fputs(char *buf);
hidden noplt void __dl_stdout_fputs_s(char *buf, size_t len);
hidden noplt size_t __dl_strlen(char *buf);
hidden noplt void __dl_puts(char* buf);
hidden noplt void __dl_print_int(int64_t n);
hidden noplt void __dl_print_hex(uint64_t n);
hidden noreturn noplt void __dl_die(char *msg);
hidden noreturn noplt void *__dl_memcpy(void* dest, const void* src, size_t n);
hidden noplt uint32_t __dl_gnu_hash_get_num_syms(uint32_t *hashtab);
