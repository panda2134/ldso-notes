#pragma once

#include <stdint.h>
#include <stddef.h>

struct tls_module {
	struct tls_module *next;
	void *image;
	size_t len, size, align, offset;
};

struct __pthread {
	/* Part 1 -- these fields may be external or
	 * internal (accessed via asm) ABI. Do not change. */
	struct pthread *self;
	uintptr_t *dtv;
	struct pthread *prev, *next;
	uintptr_t sysinfo;
	uintptr_t canary;

	// The rest are used by libc itself.
	char __padding[200 - sizeof(uintptr_t) * 6];
};

#define pthread __pthread

typedef struct __pthread * __pthread_t;
