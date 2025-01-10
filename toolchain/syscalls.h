#pragma once
#include "utils.h"

hidden noplt void * __dl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
hidden noplt int __dl_munmap(void *addr, size_t len);
hidden noplt int __dl_mprotect(void *addr, size_t len, int prot);
hidden noplt int __dl_open(const char *pathname, int flags, int mode);
hidden noplt int __dl_close(int fd);
hidden noplt int __dl_fstat(int fd, struct stat* buf);
