#pragma once
#include "utils.h"
#include "syscalls.h"

#define POOL_SIZE 4096

/**
 * We implement our own "home-brew" version of mallocs. No free() call is implemented.
 * We maintain a single-page pool of memory. If the desired bytes are more than a page,
 * an mmap() call will be issued. Otherwise, an attempt will be made to allocate bytes
 * from the pool.
 */

typedef struct malloc_info {
    char *pool_cur, *pool_end;
} MallocInfo;

void* __dl_malloc(size_t nbytes);