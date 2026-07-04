
#ifndef QEMU_INTERVAL_TREE_H
#define QEMU_INTERVAL_TREE_H

typedef struct RBNode
{
    uintptr_t rb_parent_color;
    struct RBNode *rb_right;
    struct RBNode *rb_left;
} RBNode;

typedef struct RBRoot
{
    RBNode *rb_node;
} RBRoot;

typedef struct RBRootLeftCached {
    RBRoot rb_root;
    RBNode *rb_leftmost;
} RBRootLeftCached;

typedef struct IntervalTreeNode
{
    RBNode rb;

    uint64_t start;    /* Start of interval */
    uint64_t last;     /* Last location _in_ interval */
    uint64_t subtree_last;
} IntervalTreeNode;

typedef RBRootLeftCached IntervalTreeRoot;

static inline bool interval_tree_is_empty(const IntervalTreeRoot *root)
{
    return root->rb_root.rb_node == NULL;
}

void interval_tree_insert(IntervalTreeNode *node, IntervalTreeRoot *root);

void interval_tree_remove(IntervalTreeNode *node, IntervalTreeRoot *root);

IntervalTreeNode *interval_tree_iter_first(IntervalTreeRoot *root,
                                           uint64_t start, uint64_t last);

IntervalTreeNode *interval_tree_iter_next(IntervalTreeNode *node,
                                          uint64_t start, uint64_t last);

#endif /* QEMU_INTERVAL_TREE_H */
