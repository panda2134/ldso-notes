#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include "auxv.h"
#include "utils.h"
#include "syscalls.h"
#include "dynload.h"

void __dl_print_rela_table(uint64_t rela_cnt, void *rela_table, Elf64_Sym *sym_table, char *str_table);
void __dl_print_rel_table(uint64_t rel_cnt, void *rel_table, Elf64_Sym *sym_table, char *str_table);