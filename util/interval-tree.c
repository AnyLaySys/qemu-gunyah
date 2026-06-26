
#include "qemu/osdep.h"
#include "qemu/interval-tree.h"
#include "qemu/atomic.h"



typedef enum RBColor
{
    RB_RED,
    RB_BLACK,
} RBColor;

typedef struct RBAugmentCallbacks {
    void (*propagate)(RBNode *node, RBNode *stop);
    void (*copy)(RBNode *old, RBNode *new);
    void (*rotate)(RBNode *old, RBNode *new);
} RBAugmentCallbacks;

static inline uintptr_t rb_pc(const RBNode *n)
{
    return qatomic_read(&n->rb_parent_color);
}

static inline void rb_set_pc(RBNode *n, uintptr_t pc)
{
    qatomic_set(&n->rb_parent_color, pc);
}

static inline RBNode *pc_parent(uintptr_t pc)
{
    return (RBNode *)(pc & ~1);
}

static inline RBNode *rb_parent(const RBNode *n)
{
    return pc_parent(rb_pc(n));
}

static inline RBNode *rb_red_parent(const RBNode *n)
{
    return (RBNode *)rb_pc(n);
}

static inline RBColor pc_color(uintptr_t pc)
{
    return (RBColor)(pc & 1);
}

static inline bool pc_is_red(uintptr_t pc)
{
    return pc_color(pc) == RB_RED;
}

static inline bool pc_is_black(uintptr_t pc)
{
    return !pc_is_red(pc);
}

static inline RBColor rb_color(const RBNode *n)
{
    return pc_color(rb_pc(n));
}

static inline bool rb_is_red(const RBNode *n)
{
    return pc_is_red(rb_pc(n));
}

static inline bool rb_is_black(const RBNode *n)
{
    return pc_is_black(rb_pc(n));
}

static inline void rb_set_black(RBNode *n)
{
    rb_set_pc(n, rb_pc(n) | RB_BLACK);
}

static inline void rb_set_parent_color(RBNode *n, RBNode *p, RBColor color)
{
    rb_set_pc(n, (uintptr_t)p | color);
}

static inline void rb_set_parent(RBNode *n, RBNode *p)
{
    rb_set_parent_color(n, p, rb_color(n));
}

static inline void rb_link_node(RBNode *node, RBNode *parent, RBNode **rb_link)
{
    node->rb_parent_color = (uintptr_t)parent;
    node->rb_left = node->rb_right = NULL;

    qatomic_set_mb(rb_link, node);
}

static RBNode *rb_next(RBNode *node)
{
    RBNode *parent;


    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left) {
            node = node->rb_left;
        }
        return node;
    }

    while ((parent = rb_parent(node)) && node == parent->rb_right) {
        node = parent;
    }

    return parent;
}

static inline void rb_change_child(RBNode *old, RBNode *new,
                                   RBNode *parent, RBRoot *root)
{
    if (!parent) {
        qatomic_set(&root->rb_node, new);
    } else if (parent->rb_left == old) {
        qatomic_set(&parent->rb_left, new);
    } else {
        qatomic_set(&parent->rb_right, new);
    }
}

static inline void rb_rotate_set_parents(RBNode *old, RBNode *new,
                                         RBRoot *root, RBColor color)
{
    uintptr_t pc = rb_pc(old);
    RBNode *parent = pc_parent(pc);

    rb_set_pc(new, pc);
    rb_set_parent_color(old, new, color);
    rb_change_child(old, new, parent, root);
}

static void rb_insert_augmented(RBNode *node, RBRoot *root,
                                const RBAugmentCallbacks *augment)
{
    RBNode *parent = rb_red_parent(node), *gparent, *tmp;

    while (true) {
        if (unlikely(!parent)) {
            rb_set_parent_color(node, NULL, RB_BLACK);
            break;
        }

        if (rb_is_black(parent)) {
            break;
        }

        gparent = rb_red_parent(parent);

        tmp = gparent->rb_right;
        if (parent != tmp) {    /* parent == gparent->rb_left */
            if (tmp && rb_is_red(tmp)) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_right;
            if (node == tmp) {
                tmp = node->rb_left;
                qatomic_set(&parent->rb_right, tmp);
                qatomic_set(&node->rb_left, parent);
                if (tmp) {
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                }
                rb_set_parent_color(parent, node, RB_RED);
                augment->rotate(parent, node);
                parent = node;
                tmp = node->rb_right;
            }

            qatomic_set(&gparent->rb_left, tmp); /* == parent->rb_right */
            qatomic_set(&parent->rb_right, gparent);
            if (tmp) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            }
            rb_rotate_set_parents(gparent, parent, root, RB_RED);
            augment->rotate(gparent, parent);
            break;
        } else {
            tmp = gparent->rb_left;
            if (tmp && rb_is_red(tmp)) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_left;
            if (node == tmp) {
                tmp = node->rb_right;
                qatomic_set(&parent->rb_left, tmp);
                qatomic_set(&node->rb_right, parent);
                if (tmp) {
                    rb_set_parent_color(tmp, parent, RB_BLACK);
                }
                rb_set_parent_color(parent, node, RB_RED);
                augment->rotate(parent, node);
                parent = node;
                tmp = node->rb_left;
            }

            qatomic_set(&gparent->rb_right, tmp); /* == parent->rb_left */
            qatomic_set(&parent->rb_left, gparent);
            if (tmp) {
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            }
            rb_rotate_set_parents(gparent, parent, root, RB_RED);
            augment->rotate(gparent, parent);
            break;
        }
    }
}

static void rb_insert_augmented_cached(RBNode *node,
                                       RBRootLeftCached *root, bool newleft,
                                       const RBAugmentCallbacks *augment)
{
    if (newleft) {
        root->rb_leftmost = node;
    }
    rb_insert_augmented(node, &root->rb_root, augment);
}

static void rb_erase_color(RBNode *parent, RBRoot *root,
                           const RBAugmentCallbacks *augment)
{
    RBNode *node = NULL, *sibling, *tmp1, *tmp2;

    while (true) {
        sibling = parent->rb_right;
        if (node != sibling) {  /* node == parent->rb_left */
            if (rb_is_red(sibling)) {
                tmp1 = sibling->rb_left;
                qatomic_set(&parent->rb_right, tmp1);
                qatomic_set(&sibling->rb_left, parent);
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                rb_rotate_set_parents(parent, sibling, root, RB_RED);
                augment->rotate(parent, sibling);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_right;
            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_left;
                if (!tmp2 || rb_is_black(tmp2)) {
                    rb_set_parent_color(sibling, parent, RB_RED);
                    if (rb_is_red(parent)) {
                        rb_set_black(parent);
                    } else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent) {
                            continue;
                        }
                    }
                    break;
                }
                tmp1 = tmp2->rb_right;
                qatomic_set(&sibling->rb_left, tmp1);
                qatomic_set(&tmp2->rb_right, sibling);
                qatomic_set(&parent->rb_right, tmp2);
                if (tmp1) {
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                }
                augment->rotate(sibling, tmp2);
                tmp1 = sibling;
                sibling = tmp2;
            }
            tmp2 = sibling->rb_left;
            qatomic_set(&parent->rb_right, tmp2);
            qatomic_set(&sibling->rb_left, parent);
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2) {
                rb_set_parent(tmp2, parent);
            }
            rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
            augment->rotate(parent, sibling);
            break;
        } else {
            sibling = parent->rb_left;
            if (rb_is_red(sibling)) {
                tmp1 = sibling->rb_right;
                qatomic_set(&parent->rb_left, tmp1);
                qatomic_set(&sibling->rb_right, parent);
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                rb_rotate_set_parents(parent, sibling, root, RB_RED);
                augment->rotate(parent, sibling);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_left;
            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_right;
                if (!tmp2 || rb_is_black(tmp2)) {
                    rb_set_parent_color(sibling, parent, RB_RED);
                    if (rb_is_red(parent)) {
                        rb_set_black(parent);
                    } else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent) {
                            continue;
                        }
                    }
                    break;
                }
                tmp1 = tmp2->rb_left;
                qatomic_set(&sibling->rb_right, tmp1);
                qatomic_set(&tmp2->rb_left, sibling);
                qatomic_set(&parent->rb_left, tmp2);
                if (tmp1) {
                    rb_set_parent_color(tmp1, sibling, RB_BLACK);
                }
                augment->rotate(sibling, tmp2);
                tmp1 = sibling;
                sibling = tmp2;
            }
            tmp2 = sibling->rb_right;
            qatomic_set(&parent->rb_left, tmp2);
            qatomic_set(&sibling->rb_right, parent);
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2) {
                rb_set_parent(tmp2, parent);
            }
            rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
            augment->rotate(parent, sibling);
            break;
        }
    }
}

static void rb_erase_augmented(RBNode *node, RBRoot *root,
                               const RBAugmentCallbacks *augment)
{
    RBNode *child = node->rb_right;
    RBNode *tmp = node->rb_left;
    RBNode *parent, *rebalance;
    uintptr_t pc;

    if (!tmp) {
        pc = rb_pc(node);
        parent = pc_parent(pc);
        rb_change_child(node, child, parent, root);
        if (child) {
            rb_set_pc(child, pc);
            rebalance = NULL;
        } else {
            rebalance = pc_is_black(pc) ? parent : NULL;
        }
        tmp = parent;
    } else if (!child) {
        pc = rb_pc(node);
        parent = pc_parent(pc);
        rb_set_pc(tmp, pc);
        rb_change_child(node, tmp, parent, root);
        rebalance = NULL;
        tmp = parent;
    } else {
        RBNode *successor = child, *child2;
        tmp = child->rb_left;
        if (!tmp) {
            parent = successor;
            child2 = successor->rb_right;

            augment->copy(node, successor);
        } else {
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);
            child2 = successor->rb_right;
            qatomic_set(&parent->rb_left, child2);
            qatomic_set(&successor->rb_right, child);
            rb_set_parent(child, successor);

            augment->copy(node, successor);
            augment->propagate(parent, successor);
        }

        tmp = node->rb_left;
        qatomic_set(&successor->rb_left, tmp);
        rb_set_parent(tmp, successor);

        pc = rb_pc(node);
        tmp = pc_parent(pc);
        rb_change_child(node, successor, tmp, root);

        if (child2) {
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = NULL;
        } else {
            rebalance = rb_is_black(successor) ? parent : NULL;
        }
        rb_set_pc(successor, pc);
        tmp = successor;
    }

    augment->propagate(tmp, NULL);

    if (rebalance) {
        rb_erase_color(rebalance, root, augment);
    }
}

static void rb_erase_augmented_cached(RBNode *node, RBRootLeftCached *root,
                                      const RBAugmentCallbacks *augment)
{
    if (root->rb_leftmost == node) {
        root->rb_leftmost = rb_next(node);
    }
    rb_erase_augmented(node, &root->rb_root, augment);
}



#define rb_to_itree(N)  container_of(N, IntervalTreeNode, rb)

static bool interval_tree_compute_max(IntervalTreeNode *node, bool exit)
{
    IntervalTreeNode *child;
    uint64_t max = node->last;

    if (node->rb.rb_left) {
        child = rb_to_itree(node->rb.rb_left);
        if (child->subtree_last > max) {
            max = child->subtree_last;
        }
    }
    if (node->rb.rb_right) {
        child = rb_to_itree(node->rb.rb_right);
        if (child->subtree_last > max) {
            max = child->subtree_last;
        }
    }
    if (exit && node->subtree_last == max) {
        return true;
    }
    node->subtree_last = max;
    return false;
}

static void interval_tree_propagate(RBNode *rb, RBNode *stop)
{
    while (rb != stop) {
        IntervalTreeNode *node = rb_to_itree(rb);
        if (interval_tree_compute_max(node, true)) {
            break;
        }
        rb = rb_parent(&node->rb);
    }
}

static void interval_tree_copy(RBNode *rb_old, RBNode *rb_new)
{
    IntervalTreeNode *old = rb_to_itree(rb_old);
    IntervalTreeNode *new = rb_to_itree(rb_new);

    new->subtree_last = old->subtree_last;
}

static void interval_tree_rotate(RBNode *rb_old, RBNode *rb_new)
{
    IntervalTreeNode *old = rb_to_itree(rb_old);
    IntervalTreeNode *new = rb_to_itree(rb_new);

    new->subtree_last = old->subtree_last;
    interval_tree_compute_max(old, false);
}

static const RBAugmentCallbacks interval_tree_augment = {
    .propagate = interval_tree_propagate,
    .copy = interval_tree_copy,
    .rotate = interval_tree_rotate,
};

void interval_tree_insert(IntervalTreeNode *node, IntervalTreeRoot *root)
{
    RBNode **link = &root->rb_root.rb_node, *rb_parent = NULL;
    uint64_t start = node->start, last = node->last;
    IntervalTreeNode *parent;
    bool leftmost = true;

    while (*link) {
        rb_parent = *link;
        parent = rb_to_itree(rb_parent);

        if (parent->subtree_last < last) {
            parent->subtree_last = last;
        }
        if (start < parent->start) {
            link = &parent->rb.rb_left;
        } else {
            link = &parent->rb.rb_right;
            leftmost = false;
        }
    }

    node->subtree_last = last;
    rb_link_node(&node->rb, rb_parent, link);
    rb_insert_augmented_cached(&node->rb, root, leftmost,
                               &interval_tree_augment);
}

void interval_tree_remove(IntervalTreeNode *node, IntervalTreeRoot *root)
{
    rb_erase_augmented_cached(&node->rb, root, &interval_tree_augment);
}


static IntervalTreeNode *interval_tree_subtree_search(IntervalTreeNode *node,
                                                      uint64_t start,
                                                      uint64_t last)
{
    while (true) {
        RBNode *tmp = qatomic_read(&node->rb.rb_left);
        if (tmp) {
            IntervalTreeNode *left = rb_to_itree(tmp);

            if (start <= left->subtree_last) {
                node = left;
                continue;
            }
        }
        if (node->start <= last) {         /* Cond1 */
            if (start <= node->last) {     /* Cond2 */
                return node; /* node is leftmost match */
            }
            tmp = qatomic_read(&node->rb.rb_right);
            if (tmp) {
                node = rb_to_itree(tmp);
                if (start <= node->subtree_last) {
                    continue;
                }
            }
        }
        return NULL; /* no match */
    }
}

IntervalTreeNode *interval_tree_iter_first(IntervalTreeRoot *root,
                                           uint64_t start, uint64_t last)
{
    IntervalTreeNode *node, *leftmost;

    if (!root || !root->rb_root.rb_node) {
        return NULL;
    }

    node = rb_to_itree(root->rb_root.rb_node);
    if (node->subtree_last < start) {
        return NULL;
    }

    leftmost = rb_to_itree(root->rb_leftmost);
    if (leftmost->start > last) {
        return NULL;
    }

    return interval_tree_subtree_search(node, start, last);
}

IntervalTreeNode *interval_tree_iter_next(IntervalTreeNode *node,
                                          uint64_t start, uint64_t last)
{
    RBNode *rb, *prev;

    rb = qatomic_read(&node->rb.rb_right);
    while (true) {
        if (rb) {
            IntervalTreeNode *right = rb_to_itree(rb);

            if (start <= right->subtree_last) {
                return interval_tree_subtree_search(right, start, last);
            }
        }

        do {
            rb = rb_parent(&node->rb);
            if (!rb) {
                return NULL;
            }
            prev = &node->rb;
            node = rb_to_itree(rb);
            rb = qatomic_read(&node->rb.rb_right);
        } while (prev == rb);

        if (last < node->start) {  /* !Cond1 */
            return NULL;
        }
        if (start <= node->last) { /* Cond2 */
            return node;
        }
    }
}

#if 0
static void debug_interval_tree_int(IntervalTreeNode *node,
                                    const char *dir, int level)
{
    printf("%4d %*s %s [%" PRIu64 ",%" PRIu64 "] subtree_last:%" PRIu64 "\n",
           level, level + 1, dir, rb_is_red(&node->rb) ? "r" : "b",
           node->start, node->last, node->subtree_last);

    if (node->rb.rb_left) {
        debug_interval_tree_int(rb_to_itree(node->rb.rb_left), "<", level + 1);
    }
    if (node->rb.rb_right) {
        debug_interval_tree_int(rb_to_itree(node->rb.rb_right), ">", level + 1);
    }
}

void debug_interval_tree(IntervalTreeNode *node);
void debug_interval_tree(IntervalTreeNode *node)
{
    if (node) {
        debug_interval_tree_int(node, "*", 0);
    } else {
        printf("null\n");
    }
}
#endif
