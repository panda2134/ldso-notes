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

hidden noplt void run_init();
hidden noplt void run_fini();
