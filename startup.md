## Startup Stages

The dynamic linker takes 3 stages to load and relocate itself.

- **Stage 1** of dynamic linker is invoked by kernel. Kernel puts "aux variables" onto the stack and passes the execution to dynamic linker.
  - **Jobs done: parsing argc/argv, setup base addr, fix relocation tables for libc.so**
  - It then parses everything in the aux variables, including: argc, argv (terminated by nullptr), aux k-v pairs (tagged by `AT_xxx` tags)
    - Note that it jumped over all environs.
    - The aux k-v pairs include information like:
      - `AT_BASE` base address of ld.so itself
      - `AT_PHNUM` number of program headers
      - `AT_PHENT` size of each program header entry
      - `AT_PHDR` start address of program headers
    - The ld.so can be invoked in two ways: either by `/path/to/ld.so executable` or by directly executing `executable`
      - In the first way, kernel does not put base address of executable into the aux vars (because the executable is ld.so itself), so the base address has to be set up manually using program headers.
  - It also gathers relocation related data from the `_DYNAMIC` symbol (which is the dynamic section for musl libc.so file)
    - The musl ld.so is merged with libc.so
    - `DT_REL` / `DT_RELSZ` in the `_DYNAMIC` section defines where to find the relocation table & how large it is
      - ld.so access the relocation table, and adds base offset to each entry
    - `DT_RELA` / `DT_RELASZ` is similar, but ld.so adds (base offset + offset in entry)
    - `DT_RELR` / `DT_RELRSZ` is similar, but for relative relocation. Such a new format seems to be [saving loads of space](https://maskray.me/blog/2021-10-31-relative-relocations-and-relr). It is reported that RELR can typically encode the same information in .rela.dyn in less than 3% space. As this format is relatively new and complicated, it would be okay to temporarily put it aside.
    - For all these, only the tags `REL_RELATIVE` and `REL_SYM_OR_REL` are processed. The latter is MIPS-only, so we only need to process `REL_RELATIVE` (which is `R_X86_64_RELATIVE` under x86-64). After fixing this, internal functions in the dynamic linker can be called.
  - After editing the relocation tables at `_DYNAMIC`, it then goes to stage 2.
- **Stage 2** of dynamic linker is invoked by stage 1.
  - **Jobs done: filling struct dso for libc, addend saving, parse other tags in dynamic section of libc, multi-threading**
    - After stage 2, the temporary TLS is set up and all relocation of libc is completed. So we can freely use libc functions.
  - It starts by filling in details of libc into `struct dso`. (in musl, libc / ld.so seem to be statically linked together)
    - `struct dso` is the node type of a doubly-linked list, containing SO file details and two pointers to the prev/next elements.
    - data in dso comes from the program header (`PT_xxx`), ELF header, aux variables on stack, and `_DYNAMIC` section
    - `struct dso` includes things like library name, rpath, got, symbol hashtables, base address, etc.
  - [x] It then does REL addend saving for ld.so (see System V ABI page 69-71)
  - It relocates ld.so again. This time, it doesn't simply add the offset, but proceeds with a complex algorithm, accounting for every symbol type.
    - Most of this algorithm is about addition/subtraction on addresses and offsets. (see System V ABI page 69-71)
  - After these steps, it calls **stage 2b)** to do multithreading-related work
    - Set up thread local storage (TLS)
    - [ ] TODO: figure out musl's pthread implementation & how ld.so handles TLS when loading a library
- **Stage 3** of dynamic linker is invoked by stage 2b)
  - **Jobs done: real work for ld.so -- load dependencies other than libc, and perform relocation on them**
    - It processes environs like `LD_PRELOAD` / `LD_LIBRARY_PATH`
    - It loads the rpath from ELF headers. It parses `$ORIGIN` which means "current executable path"
      - Loads of string operations are included to construct paths. This would benefit from a memory-safe language.
    - It searches for library names within RPATH and opens/mmaps it.
    - It initializes vDSO from the address provided in aux vars by kernel
    - It sorts `__init` functions in the topological order for later calling
    - It does the relocation for all deps, and then the application
      - [ ] TODO: why ld.so is relocated again? (stage 2 sets `ldso.relocated=0` even after relocation)
    - **FINAL STEP**: it jumps to address at `AT_ENTRY` and gives control to application

## References

- How to write shared libraries: https://www.akkadia.org/drepper/dsohowto.pdf
- Dynamic linking: https://refspecs.linuxbase.org/elf/gabi4+/ch5.dynamic.html
- Thread-local storage & Dynamic linking: https://maskray.me/blog/2021-02-14-all-about-thread-local-storage
- Structure of aux vectors: https://articles.manugarg.com/aboutelfauxiliaryvectors
- vDSO: https://man7.org/linux/man-pages/man7/vdso.7.html