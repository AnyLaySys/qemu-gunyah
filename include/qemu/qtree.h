


#ifndef QEMU_QTREE_H
#define QEMU_QTREE_H


#ifdef HAVE_GLIB_WITH_SLICE_ALLOCATOR

typedef struct _QTree  QTree;

typedef struct _QTreeNode QTreeNode;

typedef gboolean (*QTraverseNodeFunc)(QTreeNode *node,
                                      gpointer user_data);

QTree *q_tree_new(GCompareFunc key_compare_func);
QTree *q_tree_new_with_data(GCompareDataFunc key_compare_func,
                            gpointer key_compare_data);
QTree *q_tree_new_full(GCompareDataFunc key_compare_func,
                       gpointer key_compare_data,
                       GDestroyNotify key_destroy_func,
                       GDestroyNotify value_destroy_func);
QTree *q_tree_ref(QTree *tree);
void q_tree_unref(QTree *tree);
void q_tree_destroy(QTree *tree);
void q_tree_insert(QTree *tree,
                   gpointer key,
                   gpointer value);
void q_tree_replace(QTree *tree,
                    gpointer key,
                    gpointer value);
gboolean q_tree_remove(QTree *tree,
                       gconstpointer key);
gboolean q_tree_steal(QTree *tree,
                      gconstpointer key);
gpointer q_tree_lookup(QTree *tree,
                       gconstpointer key);
gboolean q_tree_lookup_extended(QTree *tree,
                                gconstpointer lookup_key,
                                gpointer *orig_key,
                                gpointer *value);
void q_tree_foreach(QTree *tree,
                    GTraverseFunc func,
                    gpointer user_data);
gpointer q_tree_search(QTree *tree,
                       GCompareFunc search_func,
                       gconstpointer user_data);
gint q_tree_height(QTree *tree);
gint q_tree_nnodes(QTree *tree);

#else /* !HAVE_GLIB_WITH_SLICE_ALLOCATOR */

typedef GTree QTree;
typedef GTreeNode QTreeNode;
typedef GTraverseNodeFunc QTraverseNodeFunc;

static inline QTree *q_tree_new(GCompareFunc key_compare_func)
{
    return g_tree_new(key_compare_func);
}

static inline QTree *q_tree_new_with_data(GCompareDataFunc key_compare_func,
                                          gpointer key_compare_data)
{
    return g_tree_new_with_data(key_compare_func, key_compare_data);
}

static inline QTree *q_tree_new_full(GCompareDataFunc key_compare_func,
                                     gpointer key_compare_data,
                                     GDestroyNotify key_destroy_func,
                                     GDestroyNotify value_destroy_func)
{
    return g_tree_new_full(key_compare_func, key_compare_data,
                           key_destroy_func, value_destroy_func);
}

static inline QTree *q_tree_ref(QTree *tree)
{
    return g_tree_ref(tree);
}

static inline void q_tree_unref(QTree *tree)
{
    g_tree_unref(tree);
}

static inline void q_tree_destroy(QTree *tree)
{
    g_tree_destroy(tree);
}

static inline void q_tree_insert(QTree *tree,
                                 gpointer key,
                                 gpointer value)
{
    g_tree_insert(tree, key, value);
}

static inline void q_tree_replace(QTree *tree,
                                  gpointer key,
                                  gpointer value)
{
    g_tree_replace(tree, key, value);
}

static inline gboolean q_tree_remove(QTree *tree,
                                     gconstpointer key)
{
    return g_tree_remove(tree, key);
}

static inline gboolean q_tree_steal(QTree *tree,
                                    gconstpointer key)
{
    return g_tree_steal(tree, key);
}

static inline gpointer q_tree_lookup(QTree *tree,
                                     gconstpointer key)
{
    return g_tree_lookup(tree, key);
}

static inline gboolean q_tree_lookup_extended(QTree *tree,
                                              gconstpointer lookup_key,
                                              gpointer *orig_key,
                                              gpointer *value)
{
    return g_tree_lookup_extended(tree, lookup_key, orig_key, value);
}

static inline void q_tree_foreach(QTree *tree,
                                  GTraverseFunc func,
                                  gpointer user_data)
{
    return g_tree_foreach(tree, func, user_data);
}

static inline gpointer q_tree_search(QTree *tree,
                                     GCompareFunc search_func,
                                     gconstpointer user_data)
{
    return g_tree_search(tree, search_func, user_data);
}

static inline gint q_tree_height(QTree *tree)
{
    return g_tree_height(tree);
}

static inline gint q_tree_nnodes(QTree *tree)
{
    return g_tree_nnodes(tree);
}

#endif /* HAVE_GLIB_WITH_SLICE_ALLOCATOR */

#endif /* QEMU_QTREE_H */
