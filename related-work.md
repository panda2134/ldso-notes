# Related Work

*Dynamic linkers.*
There are several previous work about dynamic linkers that affected our design.
[Symbol resolution MatRs] focused on optimization of dynamic linking by a technique they call "stable linking",
 which streamlines dynamic linking by constructing the relocation mapping ahead of time when new software is being installed.
Then, when executables are being run, symbol lookup doesn't need to be carried out, thus reducing load time of programs.
Their linker is modified from the stock linker of musl, thus compatible with existing programs.
Inspired by them, our verified linker is also based on musl for verification purposes.
[From Dynamic Loading to Extensible Transformation] takes an LLVM-like approach to make a pass-based extensible dynamic linker, called iFed.
By breaking dynamic linking into passes, dynamic linkers can be extended in a modular and flexible way.
For instance, it becomes easier to add performance optimizations into the dynamic linker.
[Dynamic Linkers Are the Narrow Waist of Operating Systems] also attempts to implement a modular dynamic linker while remaining with existing systems that uses glibc.
Their focus lies on leveraging dynamic linkers to ensure application compatibility when the operating system is constantly evolving.

*Verification tools.*
[StarMalloc] is a verified hardened memory allocator using the language F\*. They complete the verification by modeling system calls like `mmap` with axioms.
Our need is similar to theirs, except for the fact that we need to use not only anonymous memory maps (to reserve virtual address ranges), but also named
memory maps that load file contents into the virtual memory space. Nevertheless, we treat system calls in a similar manner of theirs.
