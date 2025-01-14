#include "walkdyn.h"
#include <linux/limits.h>

hidden noplt DlElfInfo* __dl_recursive_load_all(DlElfInfo *exec, EnvLdConfig *conf) {
    VisNode *vis[VIS_HASHTAB_LEN];

    DlElfInfo *start_list = 0;
    DlElfInfo **start_list_tail = &start_list;

    DlElfInfo *topo_list = 0;
    DlElfInfo **topo_list_end = &topo_list;

    SLNode *lib_path = 0;
    SLNode **lib_path_tail = &lib_path;

    __dl_memset(vis, 0, sizeof(vis));

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
    SL_APPEND("/lib", lib_path_tail);
    SL_APPEND("/usr/lib", lib_path_tail);
    SL_APPEND("/usr/local/lib", lib_path_tail);

#define H(dev_val, ino_val) ((dev_val) * 1000003 + (ino_val))

#define HASHTAB_INSERT(tbl, len, dev_val, ino_val) do { \
    uint32_t h = H(dev_val, ino_val) % (len);\
    VisNode *n = __dl_malloc(sizeof(VisNode));\
    n->next = (tbl)[h];\
    n->dev = (dev_val);\
    n->ino = (ino_val);\
    (tbl)[h] = n;\
} while (0)

    // Load everything in LD_PRELOAD before exec
    if (conf->preload) {
        __dl_puts("Parse LD_PRELOAD");
        for (SLNode *p = __dl_parse_comma_list(conf->preload); p; p = p->next) {
            SLNode *loadlist = __dl_populate_load_list(p->s, lib_path, 0);
            for (; loadlist; loadlist = loadlist->next) {
                DlElfInfo *preload_elf = __dl_loadelf(loadlist->s);
                if (preload_elf) {
                    DL_APPEND_NODE(preload_elf, start_list_tail);
                    HASHTAB_INSERT(vis, VIS_HASHTAB_LEN, preload_elf->dev, preload_elf->ino);
                }
            }
        }
    }
    DL_APPEND_NODE(exec, start_list_tail);
    HASHTAB_INSERT(vis, VIS_HASHTAB_LEN, exec->dev, exec->ino);


    for (DlElfInfo *e = start_list; e; e = e->next) {
        __dl_stdout_fputs("start list (ino) => "); __dl_print_int(e->ino);
        __dl_load_dfs(e, lib_path, vis, topo_list_end);
    }

    // topological order; leaves go first
    return topo_list;
}

hidden noplt void __dl_load_dfs(DlElfInfo *u, SLNode *lib_path, VisNode *vis[], DlElfInfo **topo_list_end) {
    __dl_stdout_fputs("loading (ino) => "); __dl_print_int(u->ino);
    u->dep_elfs = 0; // we will build edges in the DFS process
    DlElfInfo **dep_elfs_tail = &u->dep_elfs;
    // traverse its dependencies
    for (SLNode *dep = u->dep_names; dep; dep = dep->next) {
        if (__dl_strcmp("ld-linux-x86-64.so.2", dep->s) == 0) continue;  // override the system loader
        
        // TODO: $ORIGIN in rpath
        SLNode *loadlist = __dl_populate_load_list(dep->s, lib_path, u->runpath);
        bool already_loaded = false, can_load = false;
        for (SLNode *nextload = loadlist; nextload; nextload = nextload->next) {
            __dl_puts(nextload->s);
            // first, search in vis[]
            struct stat statbuf;
            if (__dl_stat(nextload->s, &statbuf) != 0) {
                __dl_stdout_fputs("ERROR: load failed: "); __dl_puts(nextload->s);
                continue;
            }
            uint64_t h = H(statbuf.st_dev, statbuf.st_ino) % VIS_HASHTAB_LEN;
            for (VisNode *node = vis[h]; node; node = node->next) {
                if (statbuf.st_dev == node->dev && statbuf.st_ino == node->ino) { // string in vis[]
                    __dl_stdout_fputs("DEBUG: already loaded: "); __dl_puts(nextload->s);
                    already_loaded = true; break;
                }
            }
            
            if (already_loaded) {
                break; // skip this library; loaded
            } else {
                __dl_stdout_fputs("DEBUG: try to load: "); __dl_puts(nextload->s);
                DlElfInfo *v = __dl_loadelf(nextload->s);
                if (v != 0) {
                    // edge exists; mark as visited
                    can_load = true;
                    HASHTAB_INSERT(vis, VIS_HASHTAB_LEN, v->dev, v->ino);
                    DL_APPEND_NODE(v, dep_elfs_tail);
                    __dl_load_dfs(v, lib_path, vis, topo_list_end);
                    break; // loaded successfully; stop searching LD_LIBRARY_PATH, etc.
                }
            }
        }
        if (!already_loaded && !can_load) {
            __dl_stdout_fputs("Cannot find shared object: "); __dl_puts(dep->s);
            __dl_die("Dependency not found");
        }
        __dl_stdout_fputs("go to next dependency of "); __dl_print_int(u->ino);
    }

    // Add self to the end of toposort list
    DL_APPEND_NODE(u, topo_list_end);
}

hidden noplt SLNode* __dl_populate_load_list(const char *path, const SLNode *lib_path, const char* runpath) {
    __dl_stdout_fputs("==> populating "); __dl_puts(path);
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

#undef H
#undef HASHTAB_INSERT