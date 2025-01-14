#include "malloc.h"

MallocInfo info;

void* __dl_malloc(size_t nbytes) {
    // Allocate a new pool page if the current pool is full.
    if (info.pool_cur >= info.pool_end) {
        void* r = __dl_mmap(0, POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (r == (void*) -1) return 0; // mmap() failed
        info.pool_cur = r;
        info.pool_end = (char*)r + POOL_SIZE;
    }

    if (info.pool_cur + nbytes > info.pool_end) { // no enough space in pool; directly allocate a new page
        void* r = __dl_mmap(0, (nbytes | 0xfff) + 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (r == (void*) -1) return 0; // mmap() failed
        else return r;
    } else { // allocating from the pool is possible
        void* r = info.pool_cur;
        info.pool_cur += nbytes;
        return r;
    }
}