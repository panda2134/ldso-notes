#include <sys/syscall.h>
	.global _start
_start:
	mov %rsp, %rdi
	call _start_c
	xorq %rdi, %rdi
	movl %eax, %edi
	movq $SYS_exit, %rax
	syscall