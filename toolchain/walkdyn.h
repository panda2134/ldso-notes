#pragma once
#include "utils.h"
#include "dynload.h"

#define DLINFO_HT_LEN 131

#define DL_H(dev, ino) (dev * 1000003 + ino)

typedef struct vis_node {
    DlElfInfo *elf;
    struct vis_node *next;
} DlInfoHTNode;

typedef struct dl_rec_result {
    DlFileInfo* topo_list;
    DlFileInfo* preload_list;
} DlRecResult;

hidden noplt void __dl_ht_insert(DlInfoHTNode *tbl[], uint64_t len, DlElfInfo *elf_val);
hidden noplt DlElfInfo *__dl_ht_lookup(DlInfoHTNode *tbl[], uint64_t len, dev_t dev, ino_t ino);
hidden noplt DlRecResult __dl_recursive_load_all(DlElfInfo *root, EnvLdConfig *conf, DlInfoHTNode *vis[]);
hidden noplt void __dl_load_dfs(DlElfInfo *u, SLNode *lib_path, DlInfoHTNode *vis[], DlFileInfo ***topo_list_end_ptr);
hidden noplt SLNode* __dl_populate_load_list(const char *path, const SLNode *lib_path, const char* runpath);
hidden noplt void __dl_relocate(DlElfInfo *elf, DlInfoHTNode **dlinfo_ht, DlFileInfo *preload, DlElfInfo *exec);
hidden noplt const Elf64_Sym * __dl_bfs_search_symbol(
    DlElfInfo *elf, const char* symname, bool skip_self,
    DlInfoHTNode **dlinfo_ht, DlFileInfo *preload, DlElfInfo **target);