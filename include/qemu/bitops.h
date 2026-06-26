
#ifndef BITOPS_H
#define BITOPS_H


#include "host-utils.h"
#include "atomic.h"

#define BITS_PER_BYTE           CHAR_BIT
#define BITS_PER_LONG           (sizeof (unsigned long) * BITS_PER_BYTE)
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define BITS_TO_U32S(nr)        DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(uint32_t))

#define BIT(nr)                 (1UL << (nr))
#define BIT_ULL(nr)             (1ULL << (nr))

#define MAKE_64BIT_MASK(shift, length) \
    (((~0ULL) >> (64 - (length))) << (shift))



#define BIT_MASK(nr)            (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)

static inline void set_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);

    *p  |= mask;
}

static inline void set_bit_atomic(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);

    qatomic_or(p, mask);
}

static inline void clear_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);

    *p &= ~mask;
}

static inline void clear_bit_atomic(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);

    return qatomic_and(p, ~mask);
}

static inline void change_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);

    *p ^= mask;
}

static inline int test_and_set_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);
    unsigned long old = *p;

    *p = old | mask;
    return (old & mask) != 0;
}

static inline int test_and_clear_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);
    unsigned long old = *p;

    *p = old & ~mask;
    return (old & mask) != 0;
}

static inline int test_and_change_bit(long nr, unsigned long *addr)
{
    unsigned long mask = BIT_MASK(nr);
    unsigned long *p = addr + BIT_WORD(nr);
    unsigned long old = *p;

    *p = old ^ mask;
    return (old & mask) != 0;
}

static inline int test_bit(long nr, const unsigned long *addr)
{
    return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

unsigned long find_last_bit(const unsigned long *addr,
                            unsigned long size);

unsigned long find_next_bit(const unsigned long *addr,
                            unsigned long size,
                            unsigned long offset);


unsigned long find_next_zero_bit(const unsigned long *addr,
                                 unsigned long size,
                                 unsigned long offset);

static inline unsigned long find_first_bit(const unsigned long *addr,
                                           unsigned long size)
{
    unsigned long result, tmp;

    for (result = 0; result < size; result += BITS_PER_LONG) {
        tmp = *addr++;
        if (tmp) {
            result += ctzl(tmp);
            return result < size ? result : size;
        }
    }
    return size;
}

static inline unsigned long find_first_zero_bit(const unsigned long *addr,
                                                unsigned long size)
{
    return find_next_zero_bit(addr, size, 0);
}


#define BIT32_MASK(nr)            (1UL << ((nr) % 32))
#define BIT32_WORD(nr)            ((nr) / 32)

static inline void set_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    *p  |= mask;
}

static inline void set_bit32_atomic(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    qatomic_or(p, mask);
}

static inline void clear_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    *p &= ~mask;
}

static inline void clear_bit32_atomic(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    return qatomic_and(p, ~mask);
}

static inline void change_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    *p ^= mask;
}

static inline int test_and_set_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);
    uint32_t old = *p;

    *p = old | mask;
    return (old & mask) != 0;
}

static inline int test_and_clear_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);
    uint32_t old = *p;

    *p = old & ~mask;
    return (old & mask) != 0;
}

static inline int test_and_change_bit32(long nr, uint32_t *addr)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);
    uint32_t old = *p;

    *p = old ^ mask;
    return (old & mask) != 0;
}

static inline int test_bit32(long nr, const uint32_t *addr)
{
    return 1U & (addr[BIT32_WORD(nr)] >> (nr & 31));
}


static inline uint8_t rol8(uint8_t word, unsigned int shift)
{
    return (word << (shift & 7)) | (word >> (-shift & 7));
}

static inline uint8_t ror8(uint8_t word, unsigned int shift)
{
    return (word >> (shift & 7)) | (word << (-shift & 7));
}

static inline uint16_t rol16(uint16_t word, unsigned int shift)
{
    return (word << (shift & 15)) | (word >> (-shift & 15));
}

static inline uint16_t ror16(uint16_t word, unsigned int shift)
{
    return (word >> (shift & 15)) | (word << (-shift & 15));
}

static inline uint32_t rol32(uint32_t word, unsigned int shift)
{
    return (word << (shift & 31)) | (word >> (-shift & 31));
}

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
    return (word >> (shift & 31)) | (word << (-shift & 31));
}

static inline uint64_t rol64(uint64_t word, unsigned int shift)
{
    return (word << (shift & 63)) | (word >> (-shift & 63));
}

static inline uint64_t ror64(uint64_t word, unsigned int shift)
{
    return (word >> (shift & 63)) | (word << (-shift & 63));
}

static inline uint32_t hswap32(uint32_t h)
{
    return rol32(h, 16);
}

static inline uint64_t hswap64(uint64_t h)
{
    uint64_t m = 0x0000ffff0000ffffull;
    h = rol64(h, 32);
    return ((h & m) << 16) | ((h >> 16) & m);
}

static inline uint64_t wswap64(uint64_t h)
{
    return rol64(h, 32);
}

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 32 - start);
    return (value >> start) & (~0U >> (32 - length));
}

static inline uint8_t extract8(uint8_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 8 - start);
    return extract32(value, start, length);
}

static inline uint16_t extract16(uint16_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 16 - start);
    return extract32(value, start, length);
}

static inline uint64_t extract64(uint64_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 64 - start);
    return (value >> start) & (~0ULL >> (64 - length));
}

static inline int32_t sextract32(uint32_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 32 - start);
    return ((int32_t)(value << (32 - length - start))) >> (32 - length);
}

static inline int64_t sextract64(uint64_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 64 - start);
    return ((int64_t)(value << (64 - length - start))) >> (64 - length);
}

static inline uint32_t deposit32(uint32_t value, int start, int length,
                                 uint32_t fieldval)
{
    uint32_t mask;
    assert(start >= 0 && length > 0 && length <= 32 - start);
    mask = (~0U >> (32 - length)) << start;
    return (value & ~mask) | ((fieldval << start) & mask);
}

static inline uint64_t deposit64(uint64_t value, int start, int length,
                                 uint64_t fieldval)
{
    uint64_t mask;
    assert(start >= 0 && length > 0 && length <= 64 - start);
    mask = (~0ULL >> (64 - length)) << start;
    return (value & ~mask) | ((fieldval << start) & mask);
}

static inline uint32_t half_shuffle32(uint32_t x)
{
    x = ((x & 0xFF00) << 8) | (x & 0x00FF);
    x = ((x << 4) | x) & 0x0F0F0F0F;
    x = ((x << 2) | x) & 0x33333333;
    x = ((x << 1) | x) & 0x55555555;
    return x;
}

static inline uint64_t half_shuffle64(uint64_t x)
{
    x = ((x & 0xFFFF0000ULL) << 16) | (x & 0xFFFF);
    x = ((x << 8) | x) & 0x00FF00FF00FF00FFULL;
    x = ((x << 4) | x) & 0x0F0F0F0F0F0F0F0FULL;
    x = ((x << 2) | x) & 0x3333333333333333ULL;
    x = ((x << 1) | x) & 0x5555555555555555ULL;
    return x;
}

static inline uint32_t half_unshuffle32(uint32_t x)
{
    x &= 0x55555555;
    x = ((x >> 1) | x) & 0x33333333;
    x = ((x >> 2) | x) & 0x0F0F0F0F;
    x = ((x >> 4) | x) & 0x00FF00FF;
    x = ((x >> 8) | x) & 0x0000FFFF;
    return x;
}

static inline uint64_t half_unshuffle64(uint64_t x)
{
    x &= 0x5555555555555555ULL;
    x = ((x >> 1) | x) & 0x3333333333333333ULL;
    x = ((x >> 2) | x) & 0x0F0F0F0F0F0F0F0FULL;
    x = ((x >> 4) | x) & 0x00FF00FF00FF00FFULL;
    x = ((x >> 8) | x) & 0x0000FFFF0000FFFFULL;
    x = ((x >> 16) | x) & 0x00000000FFFFFFFFULL;
    return x;
}

#endif
