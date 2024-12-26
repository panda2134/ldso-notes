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

## Our design choices

To introduce our design choices, we first need to introduce the following concepts.

### `DT_TEXTREL`

`DT_TEXTREL` means the dynamic linker has to inspect the `.text` section, and fill in the missing extern function / variable addresses on the fly. Most modern executable does **not** define the `DT_TEXTREL` property, and their `.text` section is read-only. The dynamic linking is done by simply filling entries in the GOT / PLTs, instead of going over `.text` (this is called PIC, position independent code).

### Static/Dynamic? v.s. PIE/non-PIE?

PIE is about executables, while PIC is about libraries.

Static executables can be both PIE (position independent executable) and non-PIE. When it is PIE, references to address of global variables use GOTs and [need to be relocated](https://github.com/sivachandra/elf-by-example/tree/master/examples/global_var_ptr). Such relocation requires no dynamic linker according to gcc man pages. (TODO: how is the relocation performed here?)

Dynamic executables can be PIE. PIE for executables are similar to PIC, but it does not use PLTs for its internal functions (TODO: how about far jumps?). Function calls are translated to pc-relative addressing. GOT is used for global variable addressing. See [this link](https://stackoverflow.com/questions/2463150/what-is-the-fpie-option-for-position-independent-executables-in-gcc-and-ld) for details. In my experiment, non-PIE dynamic executables cannot be made on x86_64-linux.

The standard `ld-linux-x86-64.so.2` is a dynamic shared object file with no `PT_INTERP` header. Therefore, it's ideal that our `ld.so` behaves the same.  However, when I attempted to compile our prototype dynamically, I cannot find a way to avoid PLTs in the generated code when calling from `start.s` to `main.c`, so we temporarily fall back to a static PIE version of ld.so (see `toolchain/` folder).

### Address Space Layout Randomization

Many claim that ASLR depends on PIE to move executables around, but ASLR was proposed in 2001, even earlier than PIE (2003). Therefore, ASLR does not inherently depend on PIE.
[Typically](https://ctf101.org/binary-exploitation/address-space-layout-randomization/), only the stack, heap, and shared libraries are ASLR enabled. From my understanding, ASLR needn't be enabled for the `ld.so` itself.

### Our choices

Our `ld.so` need not support ASLR. To avoid linking issues, we make it a statically-linked PIE executable. Because it depends on nothing external, the only self-relocation required is about its global variables. If the kernel always put the ld.so at its desired base address, no relocation would be needed at all, but whether this is always true remains to be investigated.