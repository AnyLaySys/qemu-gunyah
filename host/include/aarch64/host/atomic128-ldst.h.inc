
#ifndef AARCH64_ATOMIC128_LDST_H
#define AARCH64_ATOMIC128_LDST_H

#include "host/cpuinfo.h"
#include "tcg/debug-assert.h"


#define HAVE_ATOMIC128_RO (cpuinfo & CPUINFO_LSE2)
#define HAVE_ATOMIC128_RW 1

static inline Int128 atomic16_read_ro(const Int128 *ptr)
{
    uint64_t l, h;

    tcg_debug_assert(HAVE_ATOMIC128_RO);
    asm("ldp %[l], %[h], %[mem]"
        : [l] "=r"(l), [h] "=r"(h) : [mem] "m"(*ptr));

    return int128_make128(l, h);
}

static inline Int128 atomic16_read_rw(Int128 *ptr)
{
    uint64_t l, h;
    uint32_t tmp;

    if (cpuinfo & CPUINFO_LSE2) {
        asm("ldp %[l], %[h], %[mem]"
            : [l] "=r"(l), [h] "=r"(h) : [mem] "m"(*ptr));
    } else {
        asm("0: ldxp %[l], %[h], %[mem]\n\t"
            "stxp %w[tmp], %[l], %[h], %[mem]\n\t"
            "cbnz %w[tmp], 0b"
            : [mem] "+m"(*ptr), [tmp] "=&r"(tmp), [l] "=&r"(l), [h] "=&r"(h));
    }

    return int128_make128(l, h);
}

static inline void atomic16_set(Int128 *ptr, Int128 val)
{
    uint64_t l = int128_getlo(val), h = int128_gethi(val);
    uint64_t t1, t2;

    if (cpuinfo & CPUINFO_LSE2) {
        asm("stp %[l], %[h], %[mem]"
            : [mem] "=m"(*ptr) : [l] "r"(l), [h] "r"(h));
    } else {
        asm("0: ldxp %[t1], %[t2], %[mem]\n\t"
            "stxp %w[t1], %[l], %[h], %[mem]\n\t"
            "cbnz %w[t1], 0b"
            : [mem] "+m"(*ptr), [t1] "=&r"(t1), [t2] "=&r"(t2)
            : [l] "r"(l), [h] "r"(h));
    }
}

#endif /* AARCH64_ATOMIC128_LDST_H */
