#include "walkdyn.h"
#include "utils.h"
#include "global.h"
#include <linux/limits.h>

hidden noplt void __dl_ht_insert(DlInfoHTNode *tbl[], uint64_t len, DlElfInfo *elf) {
    uint64_t h = DL_H(elf->dev, elf->ino) % len;
    DlInfoHTNode *n = __dl_malloc(sizeof(DlInfoHTNode));
    n->next = tbl[h];
    n->elf = elf;
    tbl[h] = n;
}

hidden noplt DlElfInfo *__dl_ht_lookup(DlInfoHTNode *tbl[], uint64_t len, dev_t dev, ino_t ino) {
    uint64_t h = DL_H(dev, ino) % len;
    for (DlInfoHTNode *node = tbl[h]; node; node = node->next) {
        if (dev == node->elf->dev && ino == node->elf->ino) { // string in dlinfo_ht[]
            return node->elf;
        }
    }
    return 0;
}

hidden noplt DlRecResult __dl_recursive_load_all(DlElfInfo *exec, EnvLdConfig *conf, DlInfoHTNode *dlinfo_ht[]) {
    DlFileInfo *preload_list = 0;
    DlFileInfo **preload_list_tail = &preload_list;

    DlFileInfo *topo_list = 0;
    DlFileInfo **topo_list_end = &topo_list;

    SLNode *lib_path = 0;
    SLNode **lib_path_tail = &lib_path;

    __dl_memset(dlinfo_ht, 0, sizeof(dlinfo_ht));

    // First, handle LD_LIBRARY_PATH
    if (conf->lib_path) {
        __dl_puts("Parse LD_LIBRARY_PATH");
        // parse the comma-separated list
        lib_path = __dl_parse_comma_list(conf->lib_path);
        // set up the end of linked list accordingly
        SL_GOTO_END(lib_path_tail);
    }
    // append default paths
    __dl_puts("Parse default search path");
    SL_APPEND("/usr/local/musl/lib", lib_path_tail);
    SL_APPEND("/usr/lib/musl/lib", lib_path_tail);
    SL_APPEND("/lib", lib_path_tail);
    SL_APPEND("/usr/lib", lib_path_tail);
    SL_APPEND("/usr/local/lib", lib_path_tail);

    // Load everything in LD_PRELOAD before exec
    if (conf->preload) {
        __dl_puts("Parse LD_PRELOAD");
        for (SLNode *p = __dl_parse_comma_list(conf->preload); p; p = p->next) {
            SLNode *loadlist = __dl_populate_load_list(p->s, lib_path, 0);
            for (; loadlist; loadlist = loadlist->next) {
                DlElfInfo *preload_elf = __dl_loadelf(loadlist->s);
                if (preload_elf) {
                    __dl_ht_insert(dlinfo_ht, DLINFO_HT_LEN, preload_elf);
                    DL_FILE_APPEND_NODE(preload_elf->dev, preload_elf->ino, preload_list_tail);
                }
            }
        }
    }
    __dl_ht_insert(dlinfo_ht, DLINFO_HT_LEN, exec);

    // First, load all preload libs
    for (DlFileInfo *e = preload_list; e; e = e->next) {
        // __dl_stdout_fputs("start list (ino) => "); __dl_print_int(e->ino);
        DlElfInfo *elf = __dl_ht_lookup(dlinfo_ht, DLINFO_HT_LEN, e->dev, e->ino);
        __dl_load_dfs(elf, lib_path, dlinfo_ht, &topo_list_end);
    }
    // Then load exec
    __dl_load_dfs(exec, lib_path, dlinfo_ht, &topo_list_end);

    // topological order; leaves go first
    DlRecResult res;
    res.topo_list = topo_list;
    res.preload_list = preload_list;
    return res;
}

hidden noplt void __dl_load_dfs(DlElfInfo *u, SLNode *lib_path, DlInfoHTNode *dlinfo_ht[], DlFileInfo ***topo_list_end_ptr) {
    u->deps = 0; // we will build edges in the DFS process
    DlFileInfo **deps_tail = &u->deps;
    // traverse its dependencies
    for (SLNode *dep = u->dep_names; dep; dep = dep->next) {
        __dl_stdout_fputs("=> Loading dependency: "); __dl_puts(dep->s);
        if (__dl_strcmp("ld-linux-x86-64.so.2", dep->s) == 0) continue;  // override the system loader

        // TODO: $ORIGIN in rpath
        SLNode *loadlist = __dl_populate_load_list(dep->s, lib_path, u->runpath);
        bool already_loaded = false, can_load = false;
        for (SLNode *nextload = loadlist; nextload; nextload = nextload->next) {
            // __dl_stdout_fputs("==> try dep path ");
            // __dl_puts(nextload->s);
            // first, search in vis[]
            struct stat statbuf;
            if (__dl_stat(nextload->s, &statbuf) != 0) {
                // Try other search paths
                // __dl_stdout_fputs("ERROR: load failed: "); __dl_puts(nextload->s);
                continue;
            }
            DlElfInfo *node = __dl_ht_lookup(dlinfo_ht, DLINFO_HT_LEN, statbuf.st_dev, statbuf.st_ino);
            if (node) { // visited
                __dl_stdout_fputs("DEBUG: already loaded: "); __dl_puts(nextload->s);
                DL_FILE_APPEND_NODE(node->dev, node->ino, deps_tail); // add edge
                already_loaded = true; break;
            }

            if (already_loaded) {
                __dl_warn("Already loaded, skipping");
                break; // skip this library; loaded
            } else {
                // __dl_stdout_fputs("DEBUG: try to load: "); __dl_puts(nextload->s);
                DlElfInfo *v = __dl_loadelf(nextload->s);
                if (v != 0) {
                    // edge exists; mark as visited
                    can_load = true;
                    __dl_ht_insert(dlinfo_ht, DLINFO_HT_LEN, v);
                    DL_FILE_APPEND_NODE(v->dev, v->ino, deps_tail);

                    __dl_load_dfs(v, lib_path, dlinfo_ht, topo_list_end_ptr);
                    break; // loaded successfully; stop searching LD_LIBRARY_PATH, etc.
                }
            }
        }
        if (!already_loaded && !can_load) {
            __dl_stdout_fputs("Cannot find shared object: "); __dl_puts(dep->s);
            __dl_die("Dependency not found");
        }
        // __dl_stdout_fputs("go to next dependency of "); __dl_print_int(u->ino);
    }

    // Add self to the end of toposort list
    // __dl_stdout_fputs("append to topo list: "); __dl_print_int(u->ino);
    DL_FILE_APPEND_NODE(u->dev, u->ino, *topo_list_end_ptr);
}

hidden noplt SLNode* __dl_populate_load_list(const char *path, const SLNode *lib_path, const char* runpath) {
    // __dl_stdout_fputs("==> populating "); __dl_puts(path);
    if (path[0] == '/') {
        SLNode *ret = __dl_malloc(sizeof(SLNode));
        ret->s = path;
        ret->next = 0;
        return ret;
    } else {
        SLNode *ret = 0;
        SLNode **ret_tail = &ret;
        while (true) {
            if (!lib_path && !runpath) break;
            const char* ls = lib_path ? lib_path->s : runpath;
            char *fullpath = __dl_malloc(PATH_MAX);

            size_t i, j;
            for (i = 0; (i < PATH_MAX-1) && ls[i]; i++)
                fullpath[i] = ls[i];
            if (i >= PATH_MAX-1) { __dl_stdout_fputs("path too long, skipped (case 1)"); continue; }
            fullpath[i] = '/';
            for (j = 0; (i+1+j < PATH_MAX-1) && path[j]; j++)
                fullpath[i+1+j] = path[j];
            if (i+1+j > PATH_MAX-1) { __dl_stdout_fputs("path too long, skipped (case 2)"); continue; }
            fullpath[i+1+j] = '\0';

            SL_APPEND(fullpath, ret_tail);

            if (lib_path) lib_path = lib_path->next;
            else runpath = 0;
        }
        return ret;
    }
}

// returns the symbol; set target to point to the ELF that contains the given symbol
hidden noplt const Elf64_Sym * __dl_bfs_search_symbol(
    DlElfInfo *elf, const char* symname, bool skip_self,
    DlInfoHTNode **dlinfo_ht, DlFileInfo *preload, DlElfInfo **target) {
#define DL_QUEUE_SIZE 512
    DlInfoHTNode **vis = __dl_malloc(sizeof(DlInfoHTNode*) * DLINFO_HT_LEN);
    __dl_memset(vis, 0, sizeof(DlInfoHTNode*) * DLINFO_HT_LEN);
    DlElfInfo *queue[DL_QUEUE_SIZE];
    int hd = 0, tl = 0; // [hd, tl)

    __dl_ht_insert(vis, DLINFO_HT_LEN, elf);
    queue[tl++] = elf;
    while (hd < tl && tl < DL_QUEUE_SIZE) {
        DlElfInfo *u = queue[hd++];

        if (hd != 1 || !skip_self) {
            const Elf64_Sym *sym = __dl_gnu_lookup(u, symname);
            if (sym != 0) {
                *target = u;
                return sym;
            }
        }

        if (hd == 1 && preload != 0) {
            for (DlFileInfo *p = preload; p; p = p->next) {
                DlElfInfo *v = __dl_ht_lookup(dlinfo_ht, DLINFO_HT_LEN, p->dev, p->ino);
                if (v != 0) {
                    __dl_ht_insert(vis, DLINFO_HT_LEN, v);
                    queue[tl++] = v;
                }
            }
        }

        for (DlFileInfo *p = u->deps; p; p = p->next) {
            DlElfInfo *v = __dl_ht_lookup(dlinfo_ht, DLINFO_HT_LEN, p->dev, p->ino);
            if (v != 0) {
                __dl_ht_insert(vis, DLINFO_HT_LEN, v);
                queue[tl++] = v;
            }
        }
    }

    if (tl >= DL_QUEUE_SIZE) __dl_die("BFS explored too many dependencies");
    *target = 0;
    return 0;
#undef DL_QUEUE_SIZE
}

hidden noplt void __dl_relocate(DlElfInfo *elf, DlInfoHTNode **dlinfo_ht, DlFileInfo *preload, DlElfInfo *exec) {
    __dl_stdout_fputs("Relocating: "); __dl_puts(elf->load_path ? elf->load_path : "NONAME");
    uint64_t dynv[DT_NUM];
    __dl_memset(dynv, 0, sizeof(dynv));
    __dl_parse_dyn(elf->dyn, dynv);

    if (dynv[DT_REL] || dynv[DT_PLTREL] == DT_REL) __dl_die("REL currently unsupported, we only support RELA");

    Elf64_Rela *reloc_entry;
    uint64_t r;
    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            // __dl_puts("Relocating RELASZ");
            r = dynv[DT_RELASZ] / sizeof(Elf64_Rela);
            reloc_entry = (void*)(elf->base + dynv[DT_RELA]);
        } else {
            // __dl_puts("Relocating PLTRELSZ");
            r = dynv[DT_PLTRELSZ] / sizeof(Elf64_Rela);
            reloc_entry = (void*)(elf->base + dynv[DT_JMPREL]);
        }
        if (r == 0) continue;
        for (; r; reloc_entry++, r--) {
            // __dl_stdout_fputs("reloc entry remaining:");
            // __dl_print_int(r);
            Elf64_Sym *local_sym = (elf->sym_table + ELF64_R_SYM(reloc_entry->r_info));
            const char* local_sym_str = elf->str_table + local_sym->st_name;

            // if (local_sym->st_name) {
            //     __dl_stdout_fputs("sym: ");
            //     __dl_puts(local_sym_str);
            // } else {
            //     __dl_puts("no sym defined");
            // }

            int64_t type = ELF64_R_TYPE(reloc_entry->r_info);
            if (type == R_X86_64_GLOB_DAT) {
                // point to symbol in exec .bss
                const Elf64_Sym *exec_sym = __dl_gnu_lookup(exec, local_sym_str);
                uint64_t *offset = (void*)(elf->base + reloc_entry->r_offset);
                if (exec_sym) {
                    *offset = exec->base + exec_sym->st_value;
                } else {
                    // Fallback to libc ones.
                    if (__dl_strcmp("__environ", local_sym_str) == 0) {
                        *offset = (uint64_t) &__environ;
                    } else if (__dl_strcmp("__progname", local_sym_str) == 0) {
                        *offset = (uint64_t) &__progname;
                    } else if (__dl_strcmp("__progname_full", local_sym_str) == 0) {
                        *offset = (uint64_t) &__progname_full;
                    } else if (__dl_strcmp("__stack_chk_guard", local_sym_str) == 0) {
                        *offset = (uint64_t) &__stack_chk_guard;
                    } else {
                        __dl_stdout_fputs(local_sym_str);
                        __dl_puts(" => no glob data found");
                        continue;
                    }
                }
            } else if (type == R_X86_64_COPY) {
                // initialize symbol in exec .bss with data from library
                if (elf != exec) __dl_die("R_X86_64_COPY should be only found in exec file");
                DlElfInfo *target_elf;
                const Elf64_Sym *sym = __dl_bfs_search_symbol(elf, local_sym_str, true, dlinfo_ht, preload, &target_elf);
                if (!sym) __dl_die("symbol not found (R_X86_64_COPY)");
                if (sym->st_size != local_sym->st_size) __dl_die("symbol size mismatch (R_X86_64_COPY)");
                __dl_memcpy(
                    (void*)(elf->base + local_sym->st_value),
                    (void*)(target_elf->base + sym->st_value),
                    sym->st_size
                );
            } else if (type == R_X86_64_RELATIVE) {
                // local_sym_str is never used here
                uint64_t *offset = (void*)(elf->base + reloc_entry->r_offset);
                *offset = elf->base + reloc_entry->r_addend;
            } else if (type == R_X86_64_JUMP_SLOT) {
                DlElfInfo *target_elf;
                const Elf64_Sym *sym = __dl_bfs_search_symbol(elf, local_sym_str, false, dlinfo_ht, preload, &target_elf);
                if (!sym) __dl_die("symbol not found (R_X86_64_JUMP_SLOT)");
                uint64_t *offset = (void*)(elf->base + reloc_entry->r_offset);
                *offset = target_elf->base + sym->st_value;
            }
        }
    }
}
