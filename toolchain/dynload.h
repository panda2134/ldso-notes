#pragma once
#include "utils.h"
#include "syscalls.h"
#include <stdbool.h>
#include <elf.h>


hidden noplt bool __dl_checkelf(Elf64_Ehdr *ehdr);
hidden noplt void* __dl_loadelf(int fd, int64_t *phnum);
