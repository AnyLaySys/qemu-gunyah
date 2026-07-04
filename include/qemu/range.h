
#ifndef QEMU_RANGE_H
#define QEMU_RANGE_H

#include "qemu/bitops.h"


struct Range {
    uint64_t lob;        /* inclusive lower bound */
    uint64_t upb;        /* inclusive upper bound */
};

static inline void range_invariant(const Range *range)
{
    assert(range->lob <= range->upb || range->lob == range->upb + 1);
}

#define range_empty ((Range){ .lob = 1, .upb = 0 })

static inline bool range_is_empty(const Range *range)
{
    range_invariant(range);
    return range->lob > range->upb;
}

static inline bool range_contains(const Range *range, uint64_t val)
{
    return val >= range->lob && val <= range->upb;
}

static inline void range_make_empty(Range *range)
{
    *range = range_empty;
    assert(range_is_empty(range));
}

static inline void range_set_bounds(Range *range, uint64_t lob, uint64_t upb)
{
    range->lob = lob;
    range->upb = upb;
    assert(!range_is_empty(range));
}

static inline void range_set_bounds1(Range *range,
                                     uint64_t lob, uint64_t upb_plus1)
{
    if (!lob && !upb_plus1) {
        *range = range_empty;
    } else {
        range->lob = lob;
        range->upb = upb_plus1 - 1;
    }
    range_invariant(range);
}

static inline uint64_t range_lob(Range *range)
{
    assert(!range_is_empty(range));
    return range->lob;
}

static inline uint64_t range_upb(Range *range)
{
    assert(!range_is_empty(range));
    return range->upb;
}

G_GNUC_WARN_UNUSED_RESULT
static inline int range_init(Range *range, uint64_t lob, uint64_t size)
{
    if (lob + size < lob) {
        return -ERANGE;
    }
    range->lob = lob;
    range->upb = lob + size - 1;
    range_invariant(range);
    return 0;
}

static inline void range_init_nofail(Range *range, uint64_t lob, uint64_t size)
{
    range->lob = lob;
    range->upb = lob + size - 1;
    range_invariant(range);
}

static inline uint64_t range_size(const Range *range)
{
    return range->upb - range->lob + 1;
}

static inline bool range_overlaps_range(const Range *range1,
                                        const Range *range2)
{
    if (range_is_empty(range1) || range_is_empty(range2)) {
        return false;
    }
    return !(range2->upb < range1->lob || range1->upb < range2->lob);
}

static inline bool range_contains_range(const Range *range1,
                                        const Range *range2)
{
    if (range_is_empty(range1) || range_is_empty(range2)) {
        return false;
    }
    return range1->lob <= range2->lob && range1->upb >= range2->upb;
}

static inline void range_extend(Range *range, Range *extend_by)
{
    if (range_is_empty(extend_by)) {
        return;
    }
    if (range_is_empty(range)) {
        *range = *extend_by;
        return;
    }
    if (range->lob > extend_by->lob) {
        range->lob = extend_by->lob;
    }
    if (range->upb < extend_by->upb) {
        range->upb = extend_by->upb;
    }
    range_invariant(range);
}

static inline uint64_t range_get_last(uint64_t offset, uint64_t len)
{
    return offset + len - 1;
}

static inline int range_covers_byte(uint64_t offset, uint64_t len,
                                    uint64_t byte)
{
    return offset <= byte && byte <= range_get_last(offset, len);
}

static inline bool ranges_overlap(uint64_t first1, uint64_t len1,
                                  uint64_t first2, uint64_t len2)
{
    uint64_t last1 = range_get_last(first1, len1);
    uint64_t last2 = range_get_last(first2, len2);

    return !(last2 < first1 || last1 < first2);
}

static inline int range_get_last_bit(Range *range)
{
    if (range_is_empty(range)) {
        return -1;
    }
    return 63 - clz64(range->upb);
}

int range_compare(Range *a, Range *b);

GList *range_list_insert(GList *list, Range *data);

void range_inverse_array(GList *in_ranges,
                         GList **out_ranges,
                         uint64_t low, uint64_t high);

#endif
