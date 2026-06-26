
#ifndef CRYPTO_CLMUL_H
#define CRYPTO_CLMUL_H

#include "qemu/int128.h"
#include "host/crypto/clmul.h"

uint64_t clmul_8x8_low(uint64_t, uint64_t);

uint64_t clmul_8x4_even(uint64_t, uint64_t);

uint64_t clmul_8x4_odd(uint64_t, uint64_t);

uint64_t clmul_8x4_packed(uint32_t, uint32_t);

uint64_t clmul_16x2_even(uint64_t, uint64_t);

uint64_t clmul_16x2_odd(uint64_t, uint64_t);

uint64_t clmul_32(uint32_t, uint32_t);

Int128 clmul_64_gen(uint64_t, uint64_t);

static inline Int128 clmul_64(uint64_t a, uint64_t b)
{
    if (HAVE_CLMUL_ACCEL) {
        return clmul_64_accel(a, b);
    } else {
        return clmul_64_gen(a, b);
    }
}

#endif /* CRYPTO_CLMUL_H */
