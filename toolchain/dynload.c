#include "dynload.h"
// Check if ELF header is valid.
hidden noplt bool __dl_checkelf(Elf64_Ehdr *ehdr) {
    return ehdr->e_ident[0] == ELFMAG0
        && ehdr->e_ident[1] == ELFMAG1
        && ehdr->e_ident[2] == ELFMAG2
        && ehdr->e_ident[3] == ELFMAG3
        && ehdr->e_ident[4] == ELFCLASS64
        && ehdr->e_ident[5] == ELFDATA2LSB
        && ehdr->e_ident[6] == EV_CURRENT
        && ehdr->e_machine == EM_X86_64
        && ehdr->e_version == EV_CURRENT
        ;
}

// Uses mmap to map segments of ELF into memory.
// Returns the phdr address.
// TODO: support ASLR
hidden noplt void* __dl_loadelf(int fd, int64_t* phnum) {
    // Read program header info from ELF header.
    Elf64_Ehdr *ehdr = __dl_mmap(0, sizeof(Elf64_Ehdr), PROT_READ, MAP_PRIVATE, fd, 0);
    if ((ehdr == (void*)-1) || !__dl_checkelf(ehdr)) {
        __dl_die("Ehdr invalid");
    }
    uint16_t elf_type = ehdr->e_type;
    off_t phoff = ehdr->e_phoff;
    *phnum = ehdr->e_phnum;
    __dl_munmap(ehdr, sizeof(Elf64_Ehdr));

    // Calculate total size of memory image
    off_t phoff_aligned = phoff & ~0xfff;
    off_t ph_align_diff =  (phoff - phoff_aligned);
    void *phdr_aligned = __dl_mmap(0, (*phnum) * sizeof(Elf64_Phdr) + ph_align_diff, PROT_READ, MAP_PRIVATE, fd, phoff_aligned);
    if (phdr_aligned == (void*)-1) __dl_die("phdr mmap failed");
    Elf64_Phdr *phdr = (void*)((char*)phdr_aligned + ph_align_diff);
    uint64_t lowaddr = 0xffffffffffffffff, highaddr = 0;
    for (int64_t i = 0; i < *phnum; i++) {
        Elf64_Phdr *e = phdr + i;
        if (e->p_memsz != 0) {
            if (lowaddr > e->p_vaddr) lowaddr = e->p_vaddr;
            if (highaddr < (e->p_vaddr + e->p_memsz)) highaddr = (e->p_vaddr + e->p_memsz);
        }
    }
    if (lowaddr >= highaddr) {
        __dl_die("MMap range is wrong");
    }
    uint64_t mmap_size = highaddr - lowaddr;

    // Two cases exist for the ELF file. If it is not relocatable (i.e., TYPE=EXEC) then we load it at 0.
    // Otherwise, it can be loaded everywhere.
    // Use anonymous MMAP to reserve the whole range, then map segments according to program header table.
    // Afterward, unmap the program header table.
    void *m = 0;
    if (elf_type == ET_DYN){
        m = __dl_mmap(0, mmap_size, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    } else if (elf_type == ET_EXEC) {
        uint64_t lowaddr_aligned = lowaddr & ~0xfff;
        m = __dl_mmap(
            (void*)lowaddr_aligned,
            mmap_size + (lowaddr - lowaddr_aligned),
            PROT_READ,
            MAP_PRIVATE | MAP_ANON | MAP_FIXED_NOREPLACE,
            -1, 0);
    } else {
        __dl_stdout_fputs("ELF Type = "); __dl_print_int(elf_type);
        __dl_die("found unsupported ELF type");
    }
    if (m == (void*)-1) __dl_die("anonymous mmap reservation failed");
    uint64_t exec_base = (uint64_t)m - lowaddr;

    void* new_phdr_addr = 0;
    for (int64_t i = 0; i < *phnum; i++) {
        Elf64_Phdr *e = phdr + i;
        if (e->p_memsz == 0) continue;
        if (e->p_type != PT_LOAD || e->p_align != 0x1000) continue; // Only handle PT_LOAD that is 4k-aligned

        int prot = 0;
        if (e->p_flags & PF_R) prot |= PROT_READ;
        if (e->p_flags & PF_W) prot |= PROT_WRITE;
        if (e->p_flags & PF_X) prot |= PROT_EXEC;

        off_t offset_aligned = e->p_offset & ~0xfff;
        off_t align_diff = (e->p_offset - offset_aligned);

        // __dl_stdout_fputs("Mapping offset (aligned) "); __dl_print_hex(offset_aligned);
        void *r = __dl_mmap(
            (void*)(exec_base + e->p_vaddr - align_diff),
            e->p_filesz + align_diff,
            prot,
            MAP_PRIVATE | MAP_FIXED,
            fd,
            offset_aligned
        );
        if (!r) __dl_die("segment mmap failed");
        int err = __dl_mprotect(r, e->p_memsz + align_diff, prot); 
        if (err) __dl_die("mprotect failed");

    }

    for (int64_t i = 0; i < *phnum; i++) {
        Elf64_Phdr *e = phdr + i;
        if (e->p_type == PT_PHDR) {
            new_phdr_addr = (void*)(exec_base + e->p_vaddr);
            break;
        }
    }
    int err = __dl_munmap(phdr_aligned, (*phnum) * sizeof(Elf64_Phdr)); // unmap old phdr
    if (err) __dl_die("unmap old phdr failed");
    return new_phdr_addr;
}