#include <sys/syscall.h>
	.global _start
_start:
	mov %rsp, %rdi
	call _start_c
	movq $SYS_exit, %rax
	xorq %rdi, %rdi
	syscall
