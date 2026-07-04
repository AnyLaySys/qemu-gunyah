
#ifndef HBITMAP_H
#define HBITMAP_H

#include "bitops.h"
#include "host-utils.h"

typedef struct HBitmap HBitmap;
typedef struct HBitmapIter HBitmapIter;

#define BITS_PER_LEVEL         (BITS_PER_LONG == 32 ? 5 : 6)

#define HBITMAP_LOG_MAX_SIZE   (BITS_PER_LONG == 32 ? 34 : 41)

#define HBITMAP_LEVELS         ((HBITMAP_LOG_MAX_SIZE / BITS_PER_LEVEL) + 1)

struct HBitmapIter {
    const HBitmap *hb;

    int granularity;

    size_t pos;

    unsigned long cur[HBITMAP_LEVELS];
};

HBitmap *hbitmap_alloc(uint64_t size, int granularity);

void hbitmap_truncate(HBitmap *hb, uint64_t size);

void hbitmap_merge(const HBitmap *a, const HBitmap *b, HBitmap *result);

bool hbitmap_empty(const HBitmap *hb);

int hbitmap_granularity(const HBitmap *hb);

uint64_t hbitmap_count(const HBitmap *hb);

void hbitmap_set(HBitmap *hb, uint64_t start, uint64_t count);

void hbitmap_reset(HBitmap *hb, uint64_t start, uint64_t count);

void hbitmap_reset_all(HBitmap *hb);

bool hbitmap_get(const HBitmap *hb, uint64_t item);

bool hbitmap_is_serializable(const HBitmap *hb);

uint64_t hbitmap_serialization_align(const HBitmap *hb);

uint64_t hbitmap_serialization_size(const HBitmap *hb,
                                    uint64_t start, uint64_t count);

void hbitmap_serialize_part(const HBitmap *hb, uint8_t *buf,
                            uint64_t start, uint64_t count);

void hbitmap_deserialize_part(HBitmap *hb, uint8_t *buf,
                              uint64_t start, uint64_t count,
                              bool finish);

void hbitmap_deserialize_zeroes(HBitmap *hb, uint64_t start, uint64_t count,
                                bool finish);

void hbitmap_deserialize_ones(HBitmap *hb, uint64_t start, uint64_t count,
                              bool finish);

void hbitmap_deserialize_finish(HBitmap *hb);

char *hbitmap_sha256(const HBitmap *bitmap, Error **errp);

void hbitmap_free(HBitmap *hb);

void hbitmap_iter_init(HBitmapIter *hbi, const HBitmap *hb, uint64_t first);

int64_t hbitmap_next_dirty(const HBitmap *hb, int64_t start, int64_t count);

int64_t hbitmap_next_zero(const HBitmap *hb, int64_t start, int64_t count);

bool hbitmap_next_dirty_area(const HBitmap *hb, int64_t start, int64_t end,
                             int64_t max_dirty_count,
                             int64_t *dirty_start, int64_t *dirty_count);

bool hbitmap_status(const HBitmap *hb, int64_t start, int64_t count,
                    int64_t *pnum);

int64_t hbitmap_iter_next(HBitmapIter *hbi);

#endif
