#ifndef QEMU_QHT_H
#define QEMU_QHT_H

#include "qemu/seqlock.h"
#include "qemu/thread.h"
#include "qemu/qdist.h"

typedef bool (*qht_cmp_func_t)(const void *a, const void *b);

struct qht {
    struct qht_map *map;
    qht_cmp_func_t cmp;
    QemuMutex lock; /* serializes setters of ht->map */
    unsigned int mode;
};

struct qht_stats {
    size_t head_buckets;
    size_t used_head_buckets;
    size_t entries;
    struct qdist chain;
    struct qdist occupancy;
};

typedef bool (*qht_lookup_func_t)(const void *obj, const void *userp);
typedef void (*qht_iter_func_t)(void *p, uint32_t h, void *up);
typedef bool (*qht_iter_bool_func_t)(void *p, uint32_t h, void *up);

#define QHT_MODE_AUTO_RESIZE 0x1 /* auto-resize when heavily loaded */
#define QHT_MODE_RAW_MUTEXES 0x2 /* bypass the profiler (QSP) */

void qht_init(struct qht *ht, qht_cmp_func_t cmp, size_t n_elems,
              unsigned int mode);

void qht_destroy(struct qht *ht);

bool qht_insert(struct qht *ht, void *p, uint32_t hash, void **existing);

void *qht_lookup_custom(const struct qht *ht, const void *userp, uint32_t hash,
                        qht_lookup_func_t func);

void *qht_lookup(const struct qht *ht, const void *userp, uint32_t hash);

bool qht_remove(struct qht *ht, const void *p, uint32_t hash);

void qht_reset(struct qht *ht);

bool qht_reset_size(struct qht *ht, size_t n_elems);

bool qht_resize(struct qht *ht, size_t n_elems);

void qht_iter(struct qht *ht, qht_iter_func_t func, void *userp);

void qht_iter_remove(struct qht *ht, qht_iter_bool_func_t func, void *userp);

void qht_statistics_init(const struct qht *ht, struct qht_stats *stats);

void qht_statistics_destroy(struct qht_stats *stats);

#endif /* QEMU_QHT_H */
