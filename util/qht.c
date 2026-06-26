#include "qemu/osdep.h"
#include "qemu/qht.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/memalign.h"


#define QHT_BUCKET_ALIGN 64

#if HOST_LONG_BITS == 32
#define QHT_BUCKET_ENTRIES 6
#else /* 64-bit */
#define QHT_BUCKET_ENTRIES 4
#endif

enum qht_iter_type {
    QHT_ITER_VOID,    /* do nothing; use retvoid */
    QHT_ITER_RM,      /* remove element if retbool returns true */
};

struct qht_iter {
    union {
        qht_iter_func_t retvoid;
        qht_iter_bool_func_t retbool;
    } f;
    enum qht_iter_type type;
};

static inline void qht_lock(struct qht *ht)
{
    if (ht->mode & QHT_MODE_RAW_MUTEXES) {
        qemu_mutex_lock__raw(&ht->lock);
    } else {
        qemu_mutex_lock(&ht->lock);
    }
}

static inline int qht_trylock(struct qht *ht)
{
    if (ht->mode & QHT_MODE_RAW_MUTEXES) {
        return qemu_mutex_trylock__raw(&(ht)->lock);
    }
    return qemu_mutex_trylock(&(ht)->lock);
}

static inline void qht_unlock(struct qht *ht)
{
    qemu_mutex_unlock(&ht->lock);
}

struct qht_bucket {
    QemuSpin lock;
    QemuSeqLock sequence;
    uint32_t hashes[QHT_BUCKET_ENTRIES];
    void *pointers[QHT_BUCKET_ENTRIES];
    struct qht_bucket *next;
} QEMU_ALIGNED(QHT_BUCKET_ALIGN);

QEMU_BUILD_BUG_ON(sizeof(struct qht_bucket) > QHT_BUCKET_ALIGN);

#define QHT_TSAN_BUCKET_LOCKS_BITS 4
#define QHT_TSAN_BUCKET_LOCKS (1 << QHT_TSAN_BUCKET_LOCKS_BITS)

struct qht_tsan_lock {
    QemuSpin lock;
} QEMU_ALIGNED(QHT_BUCKET_ALIGN);

struct qht_map {
    struct rcu_head rcu;
    struct qht_bucket *buckets;
    size_t n_buckets;
    size_t n_added_buckets;
    size_t n_added_buckets_threshold;
#ifdef CONFIG_TSAN
    struct qht_tsan_lock tsan_bucket_locks[QHT_TSAN_BUCKET_LOCKS];
#endif
};

#define QHT_NR_ADDED_BUCKETS_THRESHOLD_DIV 8

static void qht_do_resize_reset(struct qht *ht, struct qht_map *new,
                                bool reset);
static void qht_grow_maybe(struct qht *ht);

#ifdef QHT_DEBUG

#define qht_debug_assert(X) do { assert(X); } while (0)

static void qht_bucket_debug__locked(struct qht_bucket *b)
{
    bool seen_empty = false;
    bool corrupt = false;
    int i;

    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (b->pointers[i] == NULL) {
                seen_empty = true;
                continue;
            }
            if (seen_empty) {
                fprintf(stderr, "%s: b: %p, pos: %i, hash: 0x%x, p: %p\n",
                        __func__, b, i, b->hashes[i], b->pointers[i]);
                corrupt = true;
            }
        }
        b = b->next;
    } while (b);
    qht_debug_assert(!corrupt);
}

static void qht_map_debug__all_locked(struct qht_map *map)
{
    int i;

    for (i = 0; i < map->n_buckets; i++) {
        qht_bucket_debug__locked(&map->buckets[i]);
    }
}
#else

#define qht_debug_assert(X) do { (void)(X); } while (0)

static inline void qht_bucket_debug__locked(struct qht_bucket *b)
{ }

static inline void qht_map_debug__all_locked(struct qht_map *map)
{ }
#endif /* QHT_DEBUG */

static inline size_t qht_elems_to_buckets(size_t n_elems)
{
    return pow2ceil(n_elems / QHT_BUCKET_ENTRIES);
}

static inline void qht_do_if_first_in_stripe(struct qht_map *map,
                                             struct qht_bucket *b,
                                             void (*func)(QemuSpin *spin))
{
#ifdef CONFIG_TSAN
    unsigned long bucket_idx = b - map->buckets;
    bool is_first_in_stripe = (bucket_idx >> QHT_TSAN_BUCKET_LOCKS_BITS) == 0;
    if (is_first_in_stripe) {
        unsigned long lock_idx = bucket_idx & (QHT_TSAN_BUCKET_LOCKS - 1);
        func(&map->tsan_bucket_locks[lock_idx].lock);
    }
#else
    func(&b->lock);
#endif
}

static inline void qht_bucket_lock_do(struct qht_map *map,
                                      struct qht_bucket *b,
                                      void (*func)(QemuSpin *lock))
{
#ifdef CONFIG_TSAN
    unsigned long bucket_idx = b - map->buckets;
    unsigned long lock_idx = bucket_idx & (QHT_TSAN_BUCKET_LOCKS - 1);
    func(&map->tsan_bucket_locks[lock_idx].lock);
#else
    func(&b->lock);
#endif
}

static inline void qht_bucket_lock(struct qht_map *map,
                                   struct qht_bucket *b)
{
    qht_bucket_lock_do(map, b, qemu_spin_lock);
}

static inline void qht_bucket_unlock(struct qht_map *map,
                                     struct qht_bucket *b)
{
    qht_bucket_lock_do(map, b, qemu_spin_unlock);
}

static inline void qht_head_init(struct qht_map *map, struct qht_bucket *b)
{
    memset(b, 0, sizeof(*b));
    qht_do_if_first_in_stripe(map, b, qemu_spin_init);
    seqlock_init(&b->sequence);
}

static inline
struct qht_bucket *qht_map_to_bucket(const struct qht_map *map, uint32_t hash)
{
    return &map->buckets[hash & (map->n_buckets - 1)];
}

static void qht_map_lock_buckets(struct qht_map *map)
{
    size_t i;

    for (i = 0; i < map->n_buckets; i++) {
        struct qht_bucket *b = &map->buckets[i];

        qht_do_if_first_in_stripe(map, b, qemu_spin_lock);
    }
}

static void qht_map_unlock_buckets(struct qht_map *map)
{
    size_t i;

    for (i = 0; i < map->n_buckets; i++) {
        struct qht_bucket *b = &map->buckets[i];

        qht_do_if_first_in_stripe(map, b, qemu_spin_unlock);
    }
}

static inline bool qht_map_is_stale__locked(const struct qht *ht,
                                            const struct qht_map *map)
{
    return map != ht->map;
}

static inline
void qht_map_lock_buckets__no_stale(struct qht *ht, struct qht_map **pmap)
{
    struct qht_map *map;

    map = qatomic_rcu_read(&ht->map);
    qht_map_lock_buckets(map);
    if (likely(!qht_map_is_stale__locked(ht, map))) {
        *pmap = map;
        return;
    }
    qht_map_unlock_buckets(map);

    qht_lock(ht);
    map = ht->map;
    qht_map_lock_buckets(map);
    qht_unlock(ht);
    *pmap = map;
    return;
}

static inline
struct qht_bucket *qht_bucket_lock__no_stale(struct qht *ht, uint32_t hash,
                                             struct qht_map **pmap)
{
    struct qht_bucket *b;
    struct qht_map *map;

    map = qatomic_rcu_read(&ht->map);
    b = qht_map_to_bucket(map, hash);

    qht_bucket_lock(map, b);
    if (likely(!qht_map_is_stale__locked(ht, map))) {
        *pmap = map;
        return b;
    }
    qht_bucket_unlock(map, b);

    qht_lock(ht);
    map = ht->map;
    b = qht_map_to_bucket(map, hash);
    qht_bucket_lock(map, b);
    qht_unlock(ht);
    *pmap = map;
    return b;
}

static inline bool qht_map_needs_resize(const struct qht_map *map)
{
    return qatomic_read(&map->n_added_buckets) >
           map->n_added_buckets_threshold;
}

static inline void qht_chain_destroy(struct qht_map *map,
                                     struct qht_bucket *head)
{
    struct qht_bucket *curr = head->next;
    struct qht_bucket *prev;

    qht_do_if_first_in_stripe(map, head, qemu_spin_destroy);
    while (curr) {
        prev = curr;
        curr = curr->next;
        qemu_vfree(prev);
    }
}

static void qht_map_destroy(struct qht_map *map)
{
    size_t i;

    for (i = 0; i < map->n_buckets; i++) {
        qht_chain_destroy(map, &map->buckets[i]);
    }
    qemu_vfree(map->buckets);
    g_free(map);
}

static struct qht_map *qht_map_create(size_t n_buckets)
{
    struct qht_map *map;
    size_t i;

    map = g_malloc(sizeof(*map));
    map->n_buckets = n_buckets;

    map->n_added_buckets = 0;
    map->n_added_buckets_threshold = n_buckets /
        QHT_NR_ADDED_BUCKETS_THRESHOLD_DIV;

    if (unlikely(map->n_added_buckets_threshold == 0)) {
        map->n_added_buckets_threshold = 1;
    }

    map->buckets = qemu_memalign(QHT_BUCKET_ALIGN,
                                 sizeof(*map->buckets) * n_buckets);
    for (i = 0; i < n_buckets; i++) {
        qht_head_init(map, &map->buckets[i]);
    }
    return map;
}

void qht_init(struct qht *ht, qht_cmp_func_t cmp, size_t n_elems,
              unsigned int mode)
{
    struct qht_map *map;
    size_t n_buckets = qht_elems_to_buckets(n_elems);

    g_assert(cmp);
    ht->cmp = cmp;
    ht->mode = mode;
    qemu_mutex_init(&ht->lock);
    map = qht_map_create(n_buckets);
    qatomic_rcu_set(&ht->map, map);
}

void qht_destroy(struct qht *ht)
{
    qht_map_destroy(ht->map);
    memset(ht, 0, sizeof(*ht));
}

static void qht_bucket_reset__locked(struct qht_bucket *head)
{
    struct qht_bucket *b = head;
    int i;

    seqlock_write_begin(&head->sequence);
    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (b->pointers[i] == NULL) {
                goto done;
            }
            qatomic_set(&b->hashes[i], 0);
            qatomic_set(&b->pointers[i], NULL);
        }
        b = b->next;
    } while (b);
 done:
    seqlock_write_end(&head->sequence);
}

static void qht_map_reset__all_locked(struct qht_map *map)
{
    size_t i;

    for (i = 0; i < map->n_buckets; i++) {
        qht_bucket_reset__locked(&map->buckets[i]);
    }
    qht_map_debug__all_locked(map);
}

void qht_reset(struct qht *ht)
{
    struct qht_map *map;

    qht_map_lock_buckets__no_stale(ht, &map);
    qht_map_reset__all_locked(map);
    qht_map_unlock_buckets(map);
}

static inline void qht_do_resize(struct qht *ht, struct qht_map *new)
{
    qht_do_resize_reset(ht, new, false);
}

static inline void qht_do_resize_and_reset(struct qht *ht, struct qht_map *new)
{
    qht_do_resize_reset(ht, new, true);
}

bool qht_reset_size(struct qht *ht, size_t n_elems)
{
    struct qht_map *new = NULL;
    struct qht_map *map;
    size_t n_buckets;

    n_buckets = qht_elems_to_buckets(n_elems);

    qht_lock(ht);
    map = ht->map;
    if (n_buckets != map->n_buckets) {
        new = qht_map_create(n_buckets);
    }
    qht_do_resize_and_reset(ht, new);
    qht_unlock(ht);

    return !!new;
}

static inline
void *qht_do_lookup(const struct qht_bucket *head, qht_lookup_func_t func,
                    const void *userp, uint32_t hash)
{
    const struct qht_bucket *b = head;
    int i;

    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (qatomic_read(&b->hashes[i]) == hash) {
                void *p = qatomic_rcu_read(&b->pointers[i]);

                if (likely(p) && likely(func(p, userp))) {
                    return p;
                }
            }
        }
        b = qatomic_rcu_read(&b->next);
    } while (b);

    return NULL;
}

static __attribute__((noinline))
void *qht_lookup__slowpath(const struct qht_bucket *b, qht_lookup_func_t func,
                           const void *userp, uint32_t hash)
{
    unsigned int version;
    void *ret;

    do {
        version = seqlock_read_begin(&b->sequence);
        ret = qht_do_lookup(b, func, userp, hash);
    } while (seqlock_read_retry(&b->sequence, version));
    return ret;
}

void *qht_lookup_custom(const struct qht *ht, const void *userp, uint32_t hash,
                        qht_lookup_func_t func)
{
    const struct qht_bucket *b;
    const struct qht_map *map;
    unsigned int version;
    void *ret;

    map = qatomic_rcu_read(&ht->map);
    b = qht_map_to_bucket(map, hash);

    version = seqlock_read_begin(&b->sequence);
    ret = qht_do_lookup(b, func, userp, hash);
    if (likely(!seqlock_read_retry(&b->sequence, version))) {
        return ret;
    }
    return qht_lookup__slowpath(b, func, userp, hash);
}

void *qht_lookup(const struct qht *ht, const void *userp, uint32_t hash)
{
    return qht_lookup_custom(ht, userp, hash, ht->cmp);
}

static void *qht_insert__locked(const struct qht *ht, struct qht_map *map,
                                struct qht_bucket *head, void *p, uint32_t hash,
                                bool *needs_resize)
{
    struct qht_bucket *b = head;
    struct qht_bucket *prev = NULL;
    struct qht_bucket *new = NULL;
    int i;

    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (b->pointers[i]) {
                if (unlikely(b->hashes[i] == hash &&
                             ht->cmp(b->pointers[i], p))) {
                    return b->pointers[i];
                }
            } else {
                goto found;
            }
        }
        prev = b;
        b = b->next;
    } while (b);

    b = qemu_memalign(QHT_BUCKET_ALIGN, sizeof(*b));
    memset(b, 0, sizeof(*b));
    new = b;
    i = 0;
    qatomic_inc(&map->n_added_buckets);
    if (unlikely(qht_map_needs_resize(map)) && needs_resize) {
        *needs_resize = true;
    }

 found:
    seqlock_write_begin(&head->sequence);
    if (new) {
        qatomic_rcu_set(&prev->next, b);
    }
    qatomic_set(&b->hashes[i], hash);
    qatomic_set(&b->pointers[i], p);
    seqlock_write_end(&head->sequence);
    return NULL;
}

static __attribute__((noinline)) void qht_grow_maybe(struct qht *ht)
{
    struct qht_map *map;

    if (qht_trylock(ht)) {
        return;
    }
    map = ht->map;
    if (qht_map_needs_resize(map)) {
        struct qht_map *new = qht_map_create(map->n_buckets * 2);

        qht_do_resize(ht, new);
    }
    qht_unlock(ht);
}

bool qht_insert(struct qht *ht, void *p, uint32_t hash, void **existing)
{
    struct qht_bucket *b;
    struct qht_map *map;
    bool needs_resize = false;
    void *prev;

    qht_debug_assert(p);

    b = qht_bucket_lock__no_stale(ht, hash, &map);
    prev = qht_insert__locked(ht, map, b, p, hash, &needs_resize);
    qht_bucket_debug__locked(b);
    qht_bucket_unlock(map, b);

    if (unlikely(needs_resize) && ht->mode & QHT_MODE_AUTO_RESIZE) {
        qht_grow_maybe(ht);
    }
    if (likely(prev == NULL)) {
        return true;
    }
    if (existing) {
        *existing = prev;
    }
    return false;
}

static inline bool qht_entry_is_last(const struct qht_bucket *b, int pos)
{
    if (pos == QHT_BUCKET_ENTRIES - 1) {
        if (b->next == NULL) {
            return true;
        }
        return b->next->pointers[0] == NULL;
    }
    return b->pointers[pos + 1] == NULL;
}

static void
qht_entry_move(struct qht_bucket *to, int i, struct qht_bucket *from, int j)
{
    qht_debug_assert(!(to == from && i == j));
    qht_debug_assert(to->pointers[i]);
    qht_debug_assert(from->pointers[j]);

    qatomic_set(&to->hashes[i], from->hashes[j]);
    qatomic_set(&to->pointers[i], from->pointers[j]);

    qatomic_set(&from->hashes[j], 0);
    qatomic_set(&from->pointers[j], NULL);
}

static inline void qht_bucket_remove_entry(struct qht_bucket *orig, int pos)
{
    struct qht_bucket *b = orig;
    struct qht_bucket *prev = NULL;
    int i;

    if (qht_entry_is_last(orig, pos)) {
        qatomic_set(&orig->hashes[pos], 0);
        qatomic_set(&orig->pointers[pos], NULL);
        return;
    }
    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (b->pointers[i]) {
                continue;
            }
            if (i > 0) {
                return qht_entry_move(orig, pos, b, i - 1);
            }
            qht_debug_assert(prev);
            return qht_entry_move(orig, pos, prev, QHT_BUCKET_ENTRIES - 1);
        }
        prev = b;
        b = b->next;
    } while (b);
    qht_entry_move(orig, pos, prev, QHT_BUCKET_ENTRIES - 1);
}

static inline
bool qht_remove__locked(struct qht_bucket *head, const void *p, uint32_t hash)
{
    struct qht_bucket *b = head;
    int i;

    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            void *q = b->pointers[i];

            if (unlikely(q == NULL)) {
                return false;
            }
            if (q == p) {
                qht_debug_assert(b->hashes[i] == hash);
                seqlock_write_begin(&head->sequence);
                qht_bucket_remove_entry(b, i);
                seqlock_write_end(&head->sequence);
                return true;
            }
        }
        b = b->next;
    } while (b);
    return false;
}

bool qht_remove(struct qht *ht, const void *p, uint32_t hash)
{
    struct qht_bucket *b;
    struct qht_map *map;
    bool ret;

    qht_debug_assert(p);

    b = qht_bucket_lock__no_stale(ht, hash, &map);
    ret = qht_remove__locked(b, p, hash);
    qht_bucket_debug__locked(b);
    qht_bucket_unlock(map, b);
    return ret;
}

static inline void qht_bucket_iter(struct qht_bucket *head,
                                   const struct qht_iter *iter, void *userp)
{
    struct qht_bucket *b = head;
    int i;

    do {
        for (i = 0; i < QHT_BUCKET_ENTRIES; i++) {
            if (b->pointers[i] == NULL) {
                return;
            }
            switch (iter->type) {
            case QHT_ITER_VOID:
                iter->f.retvoid(b->pointers[i], b->hashes[i], userp);
                break;
            case QHT_ITER_RM:
                if (iter->f.retbool(b->pointers[i], b->hashes[i], userp)) {
                    seqlock_write_begin(&head->sequence);
                    qht_bucket_remove_entry(b, i);
                    seqlock_write_end(&head->sequence);
                    qht_bucket_debug__locked(b);
                    i--;
                    continue;
                }
                break;
            default:
                g_assert_not_reached();
            }
        }
        b = b->next;
    } while (b);
}

static inline void qht_map_iter__all_locked(struct qht_map *map,
                                            const struct qht_iter *iter,
                                            void *userp)
{
    size_t i;

    for (i = 0; i < map->n_buckets; i++) {
        qht_bucket_iter(&map->buckets[i], iter, userp);
    }
}

static inline void
do_qht_iter(struct qht *ht, const struct qht_iter *iter, void *userp)
{
    struct qht_map *map;

    map = qatomic_rcu_read(&ht->map);
    qht_map_lock_buckets(map);
    qht_map_iter__all_locked(map, iter, userp);
    qht_map_unlock_buckets(map);
}

void qht_iter(struct qht *ht, qht_iter_func_t func, void *userp)
{
    const struct qht_iter iter = {
        .f.retvoid = func,
        .type = QHT_ITER_VOID,
    };

    do_qht_iter(ht, &iter, userp);
}

void qht_iter_remove(struct qht *ht, qht_iter_bool_func_t func, void *userp)
{
    const struct qht_iter iter = {
        .f.retbool = func,
        .type = QHT_ITER_RM,
    };

    do_qht_iter(ht, &iter, userp);
}

struct qht_map_copy_data {
    struct qht *ht;
    struct qht_map *new;
};

static void qht_map_copy(void *p, uint32_t hash, void *userp)
{
    struct qht_map_copy_data *data = userp;
    struct qht *ht = data->ht;
    struct qht_map *new = data->new;
    struct qht_bucket *b = qht_map_to_bucket(new, hash);

    qht_insert__locked(ht, new, b, p, hash, NULL);
}

static void qht_do_resize_reset(struct qht *ht, struct qht_map *new, bool reset)
{
    struct qht_map *old;
    const struct qht_iter iter = {
        .f.retvoid = qht_map_copy,
        .type = QHT_ITER_VOID,
    };
    struct qht_map_copy_data data;

    old = ht->map;
    qht_map_lock_buckets(old);

    if (reset) {
        qht_map_reset__all_locked(old);
    }

    if (new == NULL) {
        qht_map_unlock_buckets(old);
        return;
    }

    g_assert(new->n_buckets != old->n_buckets);
    data.ht = ht;
    data.new = new;
    qht_map_iter__all_locked(old, &iter, &data);
    qht_map_debug__all_locked(new);

    qatomic_rcu_set(&ht->map, new);
    qht_map_unlock_buckets(old);
    call_rcu(old, qht_map_destroy, rcu);
}

bool qht_resize(struct qht *ht, size_t n_elems)
{
    size_t n_buckets = qht_elems_to_buckets(n_elems);
    size_t ret = false;

    qht_lock(ht);
    if (n_buckets != ht->map->n_buckets) {
        struct qht_map *new;

        new = qht_map_create(n_buckets);
        qht_do_resize(ht, new);
        ret = true;
    }
    qht_unlock(ht);

    return ret;
}

void qht_statistics_init(const struct qht *ht, struct qht_stats *stats)
{
    const struct qht_map *map;
    int i;

    map = qatomic_rcu_read(&ht->map);

    stats->used_head_buckets = 0;
    stats->entries = 0;
    qdist_init(&stats->chain);
    qdist_init(&stats->occupancy);
    if (unlikely(map == NULL)) {
        stats->head_buckets = 0;
        return;
    }
    stats->head_buckets = map->n_buckets;

    for (i = 0; i < map->n_buckets; i++) {
        const struct qht_bucket *head = &map->buckets[i];
        const struct qht_bucket *b;
        unsigned int version;
        size_t buckets;
        size_t entries;
        int j;

        do {
            version = seqlock_read_begin(&head->sequence);
            buckets = 0;
            entries = 0;
            b = head;
            do {
                for (j = 0; j < QHT_BUCKET_ENTRIES; j++) {
                    if (qatomic_read(&b->pointers[j]) == NULL) {
                        break;
                    }
                    entries++;
                }
                buckets++;
                b = qatomic_rcu_read(&b->next);
            } while (b);
        } while (seqlock_read_retry(&head->sequence, version));

        if (entries) {
            qdist_inc(&stats->chain, buckets);
            qdist_inc(&stats->occupancy,
                      (double)entries / QHT_BUCKET_ENTRIES / buckets);
            stats->used_head_buckets++;
            stats->entries += entries;
        } else {
            qdist_inc(&stats->occupancy, 0);
        }
    }
}

void qht_statistics_destroy(struct qht_stats *stats)
{
    qdist_destroy(&stats->occupancy);
    qdist_destroy(&stats->chain);
}
