#pragma once
#include "stdint.h"
#include "stddef.h"

struct __libc {
	char can_do_threads;
	char threaded;
	char secure;
	volatile signed char need_locks;
	int threads_minus_1;
	size_t *auxv;
	struct tls_module *tls_head;
	size_t tls_size, tls_align, tls_cnt;
	size_t page_size;
	char __padding[48]; // locale struct, omitted
};

extern char **__environ;
extern char *__progname, *__progname_full;
extern uintptr_t __stack_chk_guard;
