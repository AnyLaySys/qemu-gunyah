
#include "qemu/osdep.h"
#include "qemu/hbitmap.h"
#include "qemu/host-utils.h"
#include "trace.h"
#include "crypto/hash.h"


struct HBitmap {
    uint64_t orig_size;

    uint64_t size;

    uint64_t count;

    int granularity;

    HBitmap *meta;

    unsigned long *levels[HBITMAP_LEVELS];

    uint64_t sizes[HBITMAP_LEVELS];
};

static unsigned long hbitmap_iter_skip_words(HBitmapIter *hbi)
{
    size_t pos = hbi->pos;
    const HBitmap *hb = hbi->hb;
    unsigned i = HBITMAP_LEVELS - 1;

    unsigned long cur;
    do {
        i--;
        pos >>= BITS_PER_LEVEL;
        cur = hbi->cur[i] & hb->levels[i][pos];
    } while (cur == 0);


    if (i == 0 && cur == (1UL << (BITS_PER_LONG - 1))) {
        return 0;
    }
    for (; i < HBITMAP_LEVELS - 1; i++) {
        assert(cur);
        pos = (pos << BITS_PER_LEVEL) + ctzl(cur);
        hbi->cur[i] = cur & (cur - 1);

        cur = hb->levels[i + 1][pos];
    }

    hbi->pos = pos;
    trace_hbitmap_iter_skip_words(hbi->hb, hbi, pos, cur);

    assert(cur);
    return cur;
}

int64_t hbitmap_iter_next(HBitmapIter *hbi)
{
    unsigned long cur = hbi->cur[HBITMAP_LEVELS - 1] &
            hbi->hb->levels[HBITMAP_LEVELS - 1][hbi->pos];
    int64_t item;

    if (cur == 0) {
        cur = hbitmap_iter_skip_words(hbi);
        if (cur == 0) {
            return -1;
        }
    }

    hbi->cur[HBITMAP_LEVELS - 1] = cur & (cur - 1);
    item = ((uint64_t)hbi->pos << BITS_PER_LEVEL) + ctzl(cur);

    return item << hbi->granularity;
}

void hbitmap_iter_init(HBitmapIter *hbi, const HBitmap *hb, uint64_t first)
{
    unsigned i, bit;
    uint64_t pos;

    hbi->hb = hb;
    pos = first >> hb->granularity;
    assert(pos < hb->size);
    hbi->pos = pos >> BITS_PER_LEVEL;
    hbi->granularity = hb->granularity;

    for (i = HBITMAP_LEVELS; i-- > 0; ) {
        bit = pos & (BITS_PER_LONG - 1);
        pos >>= BITS_PER_LEVEL;

        hbi->cur[i] = hb->levels[i][pos] & ~((1UL << bit) - 1);

        if (i != HBITMAP_LEVELS - 1) {
            hbi->cur[i] &= ~(1UL << bit);
        }
    }
}

int64_t hbitmap_next_dirty(const HBitmap *hb, int64_t start, int64_t count)
{
    HBitmapIter hbi;
    int64_t first_dirty_off;
    uint64_t end;

    assert(start >= 0 && count >= 0);

    if (start >= hb->orig_size || count == 0) {
        return -1;
    }

    end = count > hb->orig_size - start ? hb->orig_size : start + count;

    hbitmap_iter_init(&hbi, hb, start);
    first_dirty_off = hbitmap_iter_next(&hbi);

    if (first_dirty_off < 0 || first_dirty_off >= end) {
        return -1;
    }

    return MAX(start, first_dirty_off);
}

int64_t hbitmap_next_zero(const HBitmap *hb, int64_t start, int64_t count)
{
    size_t pos = (start >> hb->granularity) >> BITS_PER_LEVEL;
    unsigned long *last_lev = hb->levels[HBITMAP_LEVELS - 1];
    unsigned long cur = last_lev[pos];
    unsigned start_bit_offset;
    uint64_t end_bit, sz;
    int64_t res;

    assert(start >= 0 && count >= 0);

    if (start >= hb->orig_size || count == 0) {
        return -1;
    }

    end_bit = count > hb->orig_size - start ?
                hb->size :
                ((start + count - 1) >> hb->granularity) + 1;
    sz = (end_bit + BITS_PER_LONG - 1) >> BITS_PER_LEVEL;

    start_bit_offset = (start >> hb->granularity) & (BITS_PER_LONG - 1);
    cur |= (1UL << start_bit_offset) - 1;
    assert((start >> hb->granularity) < hb->size);

    if (cur == (unsigned long)-1) {
        do {
            pos++;
        } while (pos < sz && last_lev[pos] == (unsigned long)-1);

        if (pos >= sz) {
            return -1;
        }

        cur = last_lev[pos];
    }

    res = (pos << BITS_PER_LEVEL) + ctol(cur);
    if (res >= end_bit) {
        return -1;
    }

    res = res << hb->granularity;
    if (res < start) {
        assert(((start - res) >> hb->granularity) == 0);
        return start;
    }

    return res;
}

bool hbitmap_next_dirty_area(const HBitmap *hb, int64_t start, int64_t end,
                             int64_t max_dirty_count,
                             int64_t *dirty_start, int64_t *dirty_count)
{
    int64_t next_zero;

    assert(start >= 0 && end >= 0 && max_dirty_count > 0);

    end = MIN(end, hb->orig_size);
    if (start >= end) {
        return false;
    }

    start = hbitmap_next_dirty(hb, start, end - start);
    if (start < 0) {
        return false;
    }

    end = start + MIN(end - start, max_dirty_count);

    next_zero = hbitmap_next_zero(hb, start, end - start);
    if (next_zero >= 0) {
        end = next_zero;
    }

    *dirty_start = start;
    *dirty_count = end - start;

    return true;
}

bool hbitmap_status(const HBitmap *hb, int64_t start, int64_t count,
                    int64_t *pnum)
{
    int64_t next_dirty, next_zero;

    assert(start >= 0);
    assert(count > 0);
    assert(start + count <= hb->orig_size);

    next_dirty = hbitmap_next_dirty(hb, start, count);
    if (next_dirty == -1) {
        *pnum = count;
        return false;
    }

    if (next_dirty > start) {
        *pnum = next_dirty - start;
        return false;
    }

    assert(next_dirty == start);

    next_zero = hbitmap_next_zero(hb, start, count);
    if (next_zero == -1) {
        *pnum = count;
        return true;
    }

    assert(next_zero > start);
    *pnum = next_zero - start;
    return true;
}

bool hbitmap_empty(const HBitmap *hb)
{
    return hb->count == 0;
}

int hbitmap_granularity(const HBitmap *hb)
{
    return hb->granularity;
}

uint64_t hbitmap_count(const HBitmap *hb)
{
    return hb->count << hb->granularity;
}

static size_t hbitmap_iter_next_word(HBitmapIter *hbi, unsigned long *p_cur)
{
    unsigned long cur = hbi->cur[HBITMAP_LEVELS - 1];

    if (cur == 0) {
        cur = hbitmap_iter_skip_words(hbi);
        if (cur == 0) {
            *p_cur = 0;
            return -1;
        }
    }

    hbi->cur[HBITMAP_LEVELS - 1] = 0;
    *p_cur = cur;
    return hbi->pos;
}

static uint64_t hb_count_between(HBitmap *hb, uint64_t start, uint64_t last)
{
    HBitmapIter hbi;
    uint64_t count = 0;
    uint64_t end = last + 1;
    unsigned long cur;
    size_t pos;

    hbitmap_iter_init(&hbi, hb, start << hb->granularity);
    for (;;) {
        pos = hbitmap_iter_next_word(&hbi, &cur);
        if (pos >= (end >> BITS_PER_LEVEL)) {
            break;
        }
        count += ctpopl(cur);
    }

    if (pos == (end >> BITS_PER_LEVEL)) {
        int bit = end & (BITS_PER_LONG - 1);
        cur &= (1UL << bit) - 1;
        count += ctpopl(cur);
    }

    return count;
}

static inline bool hb_set_elem(unsigned long *elem, uint64_t start, uint64_t last)
{
    unsigned long mask;
    unsigned long old;

    assert((last >> BITS_PER_LEVEL) == (start >> BITS_PER_LEVEL));
    assert(start <= last);

    mask = 2UL << (last & (BITS_PER_LONG - 1));
    mask -= 1UL << (start & (BITS_PER_LONG - 1));
    old = *elem;
    *elem |= mask;
    return old != *elem;
}

static bool hb_set_between(HBitmap *hb, int level, uint64_t start,
                           uint64_t last)
{
    size_t pos = start >> BITS_PER_LEVEL;
    size_t lastpos = last >> BITS_PER_LEVEL;
    bool changed = false;
    size_t i;

    i = pos;
    if (i < lastpos) {
        uint64_t next = (start | (BITS_PER_LONG - 1)) + 1;
        changed |= hb_set_elem(&hb->levels[level][i], start, next - 1);
        for (;;) {
            start = next;
            next += BITS_PER_LONG;
            if (++i == lastpos) {
                break;
            }
            changed |= (hb->levels[level][i] == 0);
            hb->levels[level][i] = ~0UL;
        }
    }
    changed |= hb_set_elem(&hb->levels[level][i], start, last);

    if (level > 0 && changed) {
        hb_set_between(hb, level - 1, pos, lastpos);
    }
    return changed;
}

void hbitmap_set(HBitmap *hb, uint64_t start, uint64_t count)
{
    uint64_t first, n;
    uint64_t last = start + count - 1;

    if (count == 0) {
        return;
    }

    trace_hbitmap_set(hb, start, count,
                      start >> hb->granularity, last >> hb->granularity);

    first = start >> hb->granularity;
    last >>= hb->granularity;
    assert(last < hb->size);
    n = last - first + 1;

    hb->count += n - hb_count_between(hb, first, last);
    if (hb_set_between(hb, HBITMAP_LEVELS - 1, first, last) &&
        hb->meta) {
        hbitmap_set(hb->meta, start, count);
    }
}

static inline bool hb_reset_elem(unsigned long *elem, uint64_t start, uint64_t last)
{
    unsigned long mask;
    bool blanked;

    assert((last >> BITS_PER_LEVEL) == (start >> BITS_PER_LEVEL));
    assert(start <= last);

    mask = 2UL << (last & (BITS_PER_LONG - 1));
    mask -= 1UL << (start & (BITS_PER_LONG - 1));
    blanked = *elem != 0 && ((*elem & ~mask) == 0);
    *elem &= ~mask;
    return blanked;
}

static bool hb_reset_between(HBitmap *hb, int level, uint64_t start,
                             uint64_t last)
{
    size_t pos = start >> BITS_PER_LEVEL;
    size_t lastpos = last >> BITS_PER_LEVEL;
    bool changed = false;
    size_t i;

    i = pos;
    if (i < lastpos) {
        uint64_t next = (start | (BITS_PER_LONG - 1)) + 1;

        if (hb_reset_elem(&hb->levels[level][i], start, next - 1)) {
            changed = true;
        } else {
            pos++;
        }

        for (;;) {
            start = next;
            next += BITS_PER_LONG;
            if (++i == lastpos) {
                break;
            }
            changed |= (hb->levels[level][i] != 0);
            hb->levels[level][i] = 0UL;
        }
    }

    if (hb_reset_elem(&hb->levels[level][i], start, last)) {
        changed = true;
    } else {
        lastpos--;
    }

    if (level > 0 && changed) {
        hb_reset_between(hb, level - 1, pos, lastpos);
    }

    return changed;

}

void hbitmap_reset(HBitmap *hb, uint64_t start, uint64_t count)
{
    uint64_t first;
    uint64_t last = start + count - 1;
    uint64_t gran = 1ULL << hb->granularity;

    if (count == 0) {
        return;
    }

    assert(QEMU_IS_ALIGNED(start, gran));
    assert(QEMU_IS_ALIGNED(count, gran) || (start + count == hb->orig_size));

    trace_hbitmap_reset(hb, start, count,
                        start >> hb->granularity, last >> hb->granularity);

    first = start >> hb->granularity;
    last >>= hb->granularity;
    assert(last < hb->size);

    hb->count -= hb_count_between(hb, first, last);
    if (hb_reset_between(hb, HBITMAP_LEVELS - 1, first, last) &&
        hb->meta) {
        hbitmap_set(hb->meta, start, count);
    }
}

void hbitmap_reset_all(HBitmap *hb)
{
    unsigned int i;

    for (i = HBITMAP_LEVELS; --i >= 1; ) {
        memset(hb->levels[i], 0, hb->sizes[i] * sizeof(unsigned long));
    }

    hb->levels[0][0] = 1UL << (BITS_PER_LONG - 1);
    hb->count = 0;
}

bool hbitmap_is_serializable(const HBitmap *hb)
{

    return hb->granularity < 58;
}

bool hbitmap_get(const HBitmap *hb, uint64_t item)
{
    uint64_t pos = item >> hb->granularity;
    unsigned long bit = 1UL << (pos & (BITS_PER_LONG - 1));
    assert(pos < hb->size);

    return (hb->levels[HBITMAP_LEVELS - 1][pos >> BITS_PER_LEVEL] & bit) != 0;
}

uint64_t hbitmap_serialization_align(const HBitmap *hb)
{
    assert(hbitmap_is_serializable(hb));

    return UINT64_C(64) << hb->granularity;
}

static void serialization_chunk(const HBitmap *hb,
                                uint64_t start, uint64_t count,
                                unsigned long **first_el, uint64_t *el_count)
{
    uint64_t last = start + count - 1;
    uint64_t gran = hbitmap_serialization_align(hb);

    assert((start & (gran - 1)) == 0);
    assert((last >> hb->granularity) < hb->size);
    if ((last >> hb->granularity) != hb->size - 1) {
        assert((count & (gran - 1)) == 0);
    }

    start = (start >> hb->granularity) >> BITS_PER_LEVEL;
    last = (last >> hb->granularity) >> BITS_PER_LEVEL;

    *first_el = &hb->levels[HBITMAP_LEVELS - 1][start];
    *el_count = last - start + 1;
}

uint64_t hbitmap_serialization_size(const HBitmap *hb,
                                    uint64_t start, uint64_t count)
{
    uint64_t el_count;
    unsigned long *cur;

    if (!count) {
        return 0;
    }
    serialization_chunk(hb, start, count, &cur, &el_count);

    return el_count * sizeof(unsigned long);
}

void hbitmap_serialize_part(const HBitmap *hb, uint8_t *buf,
                            uint64_t start, uint64_t count)
{
    uint64_t el_count;
    unsigned long *cur, *end;

    if (!count) {
        return;
    }
    serialization_chunk(hb, start, count, &cur, &el_count);
    end = cur + el_count;

    while (cur != end) {
        unsigned long el =
            (BITS_PER_LONG == 32 ? cpu_to_le32(*cur) : cpu_to_le64(*cur));

        memcpy(buf, &el, sizeof(el));
        buf += sizeof(el);
        cur++;
    }
}

void hbitmap_deserialize_part(HBitmap *hb, uint8_t *buf,
                              uint64_t start, uint64_t count,
                              bool finish)
{
    uint64_t el_count;
    unsigned long *cur, *end;

    if (!count) {
        return;
    }
    serialization_chunk(hb, start, count, &cur, &el_count);
    end = cur + el_count;

    while (cur != end) {
        memcpy(cur, buf, sizeof(*cur));

        if (BITS_PER_LONG == 32) {
            le32_to_cpus((uint32_t *)cur);
        } else {
            le64_to_cpus((uint64_t *)cur);
        }

        buf += sizeof(unsigned long);
        cur++;
    }
    if (finish) {
        hbitmap_deserialize_finish(hb);
    }
}

void hbitmap_deserialize_zeroes(HBitmap *hb, uint64_t start, uint64_t count,
                                bool finish)
{
    uint64_t el_count;
    unsigned long *first;

    if (!count) {
        return;
    }
    serialization_chunk(hb, start, count, &first, &el_count);

    memset(first, 0, el_count * sizeof(unsigned long));
    if (finish) {
        hbitmap_deserialize_finish(hb);
    }
}

void hbitmap_deserialize_ones(HBitmap *hb, uint64_t start, uint64_t count,
                              bool finish)
{
    uint64_t el_count;
    unsigned long *first;

    if (!count) {
        return;
    }
    serialization_chunk(hb, start, count, &first, &el_count);

    memset(first, 0xff, el_count * sizeof(unsigned long));
    if (finish) {
        hbitmap_deserialize_finish(hb);
    }
}

void hbitmap_deserialize_finish(HBitmap *bitmap)
{
    int64_t i, size, prev_size;
    int lev;

    size = MAX((bitmap->size + BITS_PER_LONG - 1) >> BITS_PER_LEVEL, 1);
    for (lev = HBITMAP_LEVELS - 1; lev-- > 0; ) {
        prev_size = size;
        size = MAX((size + BITS_PER_LONG - 1) >> BITS_PER_LEVEL, 1);
        memset(bitmap->levels[lev], 0, size * sizeof(unsigned long));

        for (i = 0; i < prev_size; ++i) {
            if (bitmap->levels[lev + 1][i]) {
                bitmap->levels[lev][i >> BITS_PER_LEVEL] |=
                    1UL << (i & (BITS_PER_LONG - 1));
            }
        }
    }

    bitmap->levels[0][0] |= 1UL << (BITS_PER_LONG - 1);
    bitmap->count = hb_count_between(bitmap, 0, bitmap->size - 1);
}

void hbitmap_free(HBitmap *hb)
{
    unsigned i;
    assert(!hb->meta);
    for (i = HBITMAP_LEVELS; i-- > 0; ) {
        g_free(hb->levels[i]);
    }
    g_free(hb);
}

HBitmap *hbitmap_alloc(uint64_t size, int granularity)
{
    HBitmap *hb = g_new0(struct HBitmap, 1);
    unsigned i;

    assert(size <= INT64_MAX);
    hb->orig_size = size;

    assert(granularity >= 0 && granularity < 64);
    size = (size + (1ULL << granularity) - 1) >> granularity;
    assert(size <= ((uint64_t)1 << HBITMAP_LOG_MAX_SIZE));

    hb->size = size;
    hb->granularity = granularity;
    for (i = HBITMAP_LEVELS; i-- > 0; ) {
        size = MAX((size + BITS_PER_LONG - 1) >> BITS_PER_LEVEL, 1);
        hb->sizes[i] = size;
        hb->levels[i] = g_new0(unsigned long, size);
    }

    assert(size == 1);
    hb->levels[0][0] |= 1UL << (BITS_PER_LONG - 1);
    return hb;
}

void hbitmap_truncate(HBitmap *hb, uint64_t size)
{
    bool shrink;
    unsigned i;
    uint64_t num_elements = size;
    uint64_t old;

    assert(size <= INT64_MAX);
    hb->orig_size = size;

    size = (size + (1ULL << hb->granularity) - 1) >> hb->granularity;
    assert(size <= ((uint64_t)1 << HBITMAP_LOG_MAX_SIZE));
    shrink = size < hb->size;

    if (size == hb->size) {
        return;
    }

    if (shrink) {
        uint64_t start = ROUND_UP(num_elements, UINT64_C(1) << hb->granularity);
        uint64_t fix_count = (hb->size << hb->granularity) - start;

        assert(fix_count);
        hbitmap_reset(hb, start, fix_count);
    }

    hb->size = size;
    for (i = HBITMAP_LEVELS; i-- > 0; ) {
        size = MAX(BITS_TO_LONGS(size), 1);
        if (hb->sizes[i] == size) {
            break;
        }
        old = hb->sizes[i];
        hb->sizes[i] = size;
        hb->levels[i] = g_renew(unsigned long, hb->levels[i], size);
        if (!shrink) {
            memset(&hb->levels[i][old], 0x00,
                   (size - old) * sizeof(*hb->levels[i]));
        }
    }
    if (hb->meta) {
        hbitmap_truncate(hb->meta, hb->size << hb->granularity);
    }
}

static void hbitmap_sparse_merge(HBitmap *dst, const HBitmap *src)
{
    int64_t offset;
    int64_t count;

    for (offset = 0;
         hbitmap_next_dirty_area(src, offset, src->orig_size, INT64_MAX,
                                 &offset, &count);
         offset += count)
    {
        hbitmap_set(dst, offset, count);
    }
}

void hbitmap_merge(const HBitmap *a, const HBitmap *b, HBitmap *result)
{
    int i;
    uint64_t j;

    assert(a->orig_size == result->orig_size);
    assert(b->orig_size == result->orig_size);

    if ((!hbitmap_count(a) && result == b) ||
        (!hbitmap_count(b) && result == a)) {
        return;
    }

    if (!hbitmap_count(a) && !hbitmap_count(b)) {
        hbitmap_reset_all(result);
        return;
    }

    if (a->granularity != b->granularity) {
        if ((a != result) && (b != result)) {
            hbitmap_reset_all(result);
        }
        if (a != result) {
            hbitmap_sparse_merge(result, a);
        }
        if (b != result) {
            hbitmap_sparse_merge(result, b);
        }
        return;
    }

    assert(a->size == b->size);
    for (i = HBITMAP_LEVELS - 1; i >= 0; i--) {
        for (j = 0; j < a->sizes[i]; j++) {
            result->levels[i][j] = a->levels[i][j] | b->levels[i][j];
        }
    }

    result->count = hb_count_between(result, 0, result->size - 1);
}

char *hbitmap_sha256(const HBitmap *bitmap, Error **errp)
{
    size_t size = bitmap->sizes[HBITMAP_LEVELS - 1] * sizeof(unsigned long);
    char *data = (char *)bitmap->levels[HBITMAP_LEVELS - 1];
    char *hash = NULL;
    qcrypto_hash_digest(QCRYPTO_HASH_ALGO_SHA256, data, size, &hash, errp);

    return hash;
}
