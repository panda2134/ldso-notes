#pragma once
#include "utils.h"
#include "dynload.h"

#define VIS_HASHTAB_LEN 131

typedef struct vis_node {
    dev_t dev;
    ino_t ino;
    struct vis_node *next;
} VisNode;

hidden noplt DlElfInfo* __dl_recursive_load_all(DlElfInfo *root, EnvLdConfig *conf);
hidden noplt void __dl_load_dfs(DlElfInfo *u, SLNode *lib_path, VisNode *vis[], DlElfInfo **topo_list_end);
hidden noplt SLNode* __dl_populate_load_list(const char *path, const SLNode *lib_path, const char* runpath);