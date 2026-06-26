
#ifndef AARCH64_LOAD_EXTRACT_AL16_AL8_H
#define AARCH64_LOAD_EXTRACT_AL16_AL8_H

#include "host/cpuinfo.h"
#include "tcg/debug-assert.h"

static inline uint64_t load_atom_extract_al16_or_al8(void *pv, int s)
{
    uintptr_t pi = (uintptr_t)pv;
    __int128_t *ptr_align = (__int128_t *)(pi & ~7);
    int shr = (pi & 7) * 8;
    uint64_t l, h;

    tcg_debug_assert(HAVE_ATOMIC128_RO);
    asm("ldp %0, %1, %2" : "=r"(l), "=r"(h) : "m"(*ptr_align));
    return (l >> shr) | (h << (-shr & 63));
}

#endif /* AARCH64_LOAD_EXTRACT_AL16_AL8_H */
