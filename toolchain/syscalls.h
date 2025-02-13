#pragma once

#include "defs.h"

hidden noplt void * __dl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
hidden noplt int __dl_munmap(void *addr, size_t len);
hidden noplt int __dl_mprotect(void *addr, size_t len, int prot);
hidden noplt int64_t __dl_read(uint32_t fd, char *buf, size_t count);
hidden noplt uint64_t __dl_lseek(uint32_t fd, uint64_t offset, uint32_t origin);
hidden noplt int __dl_open(const char *pathname, int flags, int mode);
hidden noplt int __dl_close(int fd);
hidden noplt int __dl_stat(const char* path, struct stat* buf);
hidden noplt int __dl_fstat(int fd, struct stat* buf);
hidden noplt int64_t __dl_readlink(const char *pathname, char *buf, size_t bufsize);
