#include "dynload.h"
#include "utils.h"
#include <elf.h>
#include <sys/fcntl.h>
#include <linux/limits.h>

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
// Returns true when load succeeded, and false when load failed.
// TODO: support ASLR
hidden noplt DlElfInfo * __dl_loadelf(const char* path) {
    __dl_stdout_fputs("Loading ELF at "); __dl_puts(path);
    DlElfInfo *ret = __dl_malloc(sizeof(DlElfInfo));

    struct stat statbuf;
    if (__dl_stat(path, &statbuf) != 0) {
        __dl_stdout_fputs("stat() failed: "); __dl_puts(path);
        return 0;
    }
    ret->dev = statbuf.st_dev; ret->ino = statbuf.st_ino;

    int fd = __dl_open(path, O_RDONLY, 0);
    if (fd < 0) {
        __dl_stdout_fputs("ERROR: open() failed: "); __dl_puts(path);
        return 0;
    }

    // Read program header info from ELF header.
    Elf64_Ehdr *ehdr = __dl_mmap(0, sizeof(Elf64_Ehdr), PROT_READ, MAP_PRIVATE, fd, 0);
    if ((ehdr == (void*)-1) || !__dl_checkelf(ehdr)) {
        __dl_stdout_fputs("ERROR: Ehdr invalid: "); __dl_puts(path);
        return 0;
    }
    uint16_t elf_type = ehdr->e_type;
    off_t phoff = ehdr->e_phoff;
    ret->elf_type = ehdr->e_type;
    ret->phnum = ehdr->e_phnum;
    ret->shoff = ehdr->e_shoff;
    ret->shnum = ehdr->e_shnum;
    ret->shentsize = ehdr->e_shentsize;
    ret->shstrndx = ehdr->e_shstrndx;
    __dl_munmap(ehdr, sizeof(Elf64_Ehdr));

    // Calculate total size of memory image
    off_t phoff_aligned = phoff & ~0xfff;
    off_t ph_align_diff =  (phoff - phoff_aligned);
    void *phdr_aligned = __dl_mmap(0, (ret->phnum) * sizeof(Elf64_Phdr) + ph_align_diff, PROT_READ, MAP_PRIVATE, fd, phoff_aligned);
    if (phdr_aligned == (void*)-1) __dl_die("phdr mmap failed");
    Elf64_Phdr *phdr = (void*)((char*)phdr_aligned + ph_align_diff);
    uint64_t lowaddr = 0xffffffffffffffff, highaddr = 0;
    for (int64_t i = 0; i < ret->phnum; i++) {
        Elf64_Phdr *e = phdr + i;
        if (e->p_type == PT_LOAD) {
            if (lowaddr > e->p_vaddr) lowaddr = e->p_vaddr;
            if (highaddr < (e->p_vaddr + e->p_memsz)) highaddr = (e->p_vaddr + e->p_memsz);
        }
    }
    if (lowaddr >= highaddr) {
        __dl_stdout_fputs("ERROR: Ehdr invalid: "); __dl_puts(path);
        return false;
    }
    uint64_t mmap_size = highaddr - lowaddr;

    // Two cases exist for the ELF file. If it is not relocatable (i.e., TYPE=EXEC) then we load it at 0.
    // Otherwise, it can be loaded everywhere.
    // Use anonymous MMAP to reserve the whole range, then map segments according to program header table.
    // Afterward, unmap the program header table.
    uint64_t lowaddr_aligned = lowaddr & ~0xfff;
    uint64_t lowaddr_align_diff = lowaddr & 0xfff;
    void *m = 0;
    if (elf_type == ET_DYN){
        __dl_puts("ET_DYN found");
        // assert that lowaddr_aligned is always 0 in PIE / PIC;
        // this assumption is used to locate program header later.
        if (lowaddr_aligned != 0) {
            __dl_stdout_fputs("ERROR: ET_DYN's lowest load address is not in page 0: "); __dl_puts(path);
            return false;
        }
        m = __dl_mmap((void*)0x800000000, mmap_size + lowaddr_align_diff, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
    } else if (elf_type == ET_EXEC) {
        __dl_puts("ET_EXEC found");
        // We're not doing self relocation. if mmap fails here, that would be because the executable
        // collides with ld.so in the memory image.
        m = __dl_mmap(
            (void*)lowaddr_aligned,
            mmap_size + lowaddr_align_diff,
            PROT_READ,
            MAP_PRIVATE | MAP_ANON | MAP_FIXED_NOREPLACE,
            -1, 0);
    } else {
        __dl_stdout_fputs("ELF Type = "); __dl_print_int(elf_type);
        __dl_stdout_fputs("ERROR: found unsupported ELF type: "); __dl_puts(path);
        return 0;
    }
    if (m == (void*)-1) __dl_die("anonymous mmap reservation failed");
    ret->base = (uint64_t)m - lowaddr_aligned;
    __dl_stdout_fputs("Reserved memory at "); __dl_print_hex(ret->base);
    if (ret->base & 0xfff) __dl_die("ret->base not 4k-aligned");
    ret->entry = (void*)(ret->base + ehdr->e_entry);

    ret->ph = 0;
    for (int64_t i = 0; i < ret->phnum; i++) {
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
            (void*)(ret->base + e->p_vaddr - align_diff),
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

    for (int64_t i = 0; i < ret->phnum; i++) {
        Elf64_Phdr *e = phdr + i;
        if (e->p_type == PT_PHDR) {
            ret->ph = (void*)(ret->base + e->p_vaddr);
            break;
        }
    }
    if (ret->ph == 0) {
        // no PT_PHDR; we will have to guess PHDR address
        ret->ph = (void*)(ret->base + phoff);
    }
    int err = __dl_munmap(phdr_aligned, (ret->phnum) * sizeof(Elf64_Phdr)); // unmap old phdr
    if (err) __dl_die("unmap old phdr failed");

    ret->load_path = path;

    if (!__dl_loadelf_extras(ret)) return 0;

    return ret;
}

hidden noplt bool __dl_loadelf_extras(DlElfInfo *ret) {
    // next, parse DT_NEEDED items, etc.
    // find the dynamic section by locating DYNAMIC segment.
    for (int64_t i = 0; i < ret->phnum; i++) {
        Elf64_Phdr *e = ret->ph + i;
        if (e->p_type == PT_DYNAMIC) {
            ret->dyn = (void*)(ret->base + e->p_vaddr);
            break;
        } else if (e->p_type == PT_TLS) {
            ret->tls.align = e->p_align;
            ret->tls.len = e->p_filesz;
            ret->tls.size = e->p_memsz;
            ret->tls.image = (void*)(ret->base + e->p_vaddr);
        }
    }

    uint64_t dynv[DT_NUM];
    __dl_memset(dynv, 0, sizeof(dynv));
    __dl_parse_dyn(ret->dyn, dynv);

    ret->str_table = (void*)(ret->base + dynv[DT_STRTAB]);
    ret->sym_table = (void*)(ret->base + dynv[DT_SYMTAB]);
    if (dynv[DT_RPATH]) __dl_die("RPATH is no longer supported");
    ret->runpath = ret->str_table + dynv[DT_RUNPATH];

    // MallocInfo info; // LEAK: this will be thrown away later.
    // __dl_memset(&info, 0, sizeof(MallocInfo));
    SLNode **deps_tail = &(ret->dep_names);
    for (Elf64_Dyn *p = ret->dyn; p->d_tag; p++) {
        if (p->d_tag == DT_NEEDED) {
            // Found dependency; insert into linked list
            SL_APPEND(ret->str_table + p->d_un.d_val, deps_tail);
        }
        if (p->d_tag == DT_GNU_HASH) {
            ret->gnu_hash_table = (void*)(ret->base + p->d_un.d_ptr);
        }
    }

    ret->relocated = false;

    return true;
}

// Map dyn items into array dynv indexed by DT_ tags. dynv[] has to be at least DT_NUM elements long.
// Extended tags like DT_GNU_HASH are ignored.
hidden noplt void __dl_parse_dyn(Elf64_Dyn *dyn, uint64_t dynv[]) {
    for (int i = 0; i < DT_NUM; i++) dynv[i] = 0;
    for (Elf64_Dyn *p = dyn; p->d_tag; p++) {
        if (p->d_tag >= DT_NUM) continue;
        dynv[p->d_tag] = (uint64_t) p->d_un.d_val;
    }
}
