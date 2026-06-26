
#ifndef SELFMAP_H
#define SELFMAP_H

#include "qemu/interval-tree.h"

typedef struct {
    IntervalTreeNode itree;

    bool is_read;
    bool is_write;
    bool is_exec;
    bool is_priv;

    dev_t dev;
    ino_t inode;
    uint64_t offset;
    const char *path;
} MapInfo;

IntervalTreeRoot *read_self_maps(void);

void free_self_maps(IntervalTreeRoot *root);

#endif /* SELFMAP_H */
