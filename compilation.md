# Compilation

## Compile options used for ld.so

### GCC

- `-nostdlib`: do not link libc and startup files.
- `-static`: forbids linking anything dynamically.

Even with those functions, gcc may introduce usage of `memcmp`, `memset`, `memcpy` and `memmove`.

### CompCert / VST

CompCert seems to be using the GCC linkers. It assumes hosted code, so we have to find a way of passing in freestanding code to VST so it can be used to verify the dynamic linker. Or maybe we only use VST to convert certain verified modules ([VSUs](https://softwarefoundations.cis.upenn.edu/vc-current/VSU_intro.html)) from .c to .o, and link them with wrappers directly using GCC.

In the VST documentation, no instruction on how to use CompCert is given. I suppose that the verification is separate from compilation. The user thus need to write a makefile for compiling manually.

### `_start`

The `_start` symbol should not be written as a C function, or the compiled code will wrongly contain assembly for setting up the stack (modifying `%rsp` / `%rbp`). This has been verified using GDB.
The correct way would be (similar to what's done with musl ld.so):

1. declare `_start` as inline assembly inserted at `.text` section
2. jump to the starting code in c, passing in the pointer for aux variables to C function. Then parse everything in C.
3. end the process with the `exit` syscall.
