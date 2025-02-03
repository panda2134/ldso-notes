# Thread-local Storage

## Introduction

### Access Models

There are four access models for TLS. They can be roughly classified into 2 categories:

- "Statically allocated TLS (IE/LE)" is pre-generating the TLS template before
_start is called. The TLS templates involved are all from libraries /
executables loaded initially. Therefore, this type of allocation cannot handle
dlopen() calls.
- “Dynamically allocated TLS (GD/LD)” allows linkage to all
types of TLS symbols, including those loaded by dlopen(). To achieve this,
`__tls_get_addr` is invoked for access to thread-local storage. This function
calculates the address of variables on-the-fly, allocating new TLS templates
whenever needed.

GD/LD are mostly used with libraries (when compiled with `-fpic` option).
IE/LE are mostly for the main executable.

The [fuschia document](https://fuchsia.dev/fuchsia-src/development/kernel/threads/tls)
is a great source for learning about different access models.

### Initial Image

The initial value of every TLS variable is included in the segment `PT_TLS` of
each ELF file. The dynamic linker maintains a list that contains reference to
all such segments. Upon creation of new thread, data from these segments are
cloned to form the TLS of the new thread.

The dynamic linker should initialize the thread-local storage before starting
the executable. This includes copying the initial image to TLS of main thread,
setting up thread-control blocks, and use `arch_prctl` to setup %fs register.
For instance, in stage 2 of musl dynamic linker, `__copy_tls` and `__init_tp`
are used for these tasks. A statically allocated space called `builtin_tls`
serves as the TLS for the main thread when ld.so is running.

### DTV and `__tls_get_addr()`

```text
*-----------------------------------------------------------------------------*
| tls_cnt | dtv[1] | ... | dtv[N] | ... | tlsN | ... | tls1 | tcb |  thread   |
*-----------------------------------------------------------------------------*
^                                       ^             ^       ^
dtv                                  dtv[n+1]       dtv[1]  tp/td
```

The entries of DTV are start addresses of thread-local storage for each module.
E.g., `dtv[i]` is the start address of TLS for module i under current thread.
Specifically, `dtv[1]` is the start address of TLS for main executable under
current thread.
