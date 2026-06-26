

#ifndef HOST_UTILS_H
#define HOST_UTILS_H

#include "qemu/bswap.h"
#include "qemu/int128.h"

#ifdef CONFIG_INT128
static inline void mulu64(uint64_t *plow, uint64_t *phigh,
                          uint64_t a, uint64_t b)
{
    __uint128_t r = (__uint128_t)a * b;
    *plow = r;
    *phigh = r >> 64;
}

static inline void muls64(uint64_t *plow, uint64_t *phigh,
                          int64_t a, int64_t b)
{
    __int128_t r = (__int128_t)a * b;
    *plow = r;
    *phigh = r >> 64;
}

static inline uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    return (__int128_t)a * b / c;
}

static inline uint64_t muldiv64_round_up(uint64_t a, uint32_t b, uint32_t c)
{
    return ((__int128_t)a * b + c - 1) / c;
}

static inline uint64_t divu128(uint64_t *plow, uint64_t *phigh,
                               uint64_t divisor)
{
    __uint128_t dividend = ((__uint128_t)*phigh << 64) | *plow;
    __uint128_t result = dividend / divisor;

    *plow = result;
    *phigh = result >> 64;
    return dividend % divisor;
}

static inline int64_t divs128(uint64_t *plow, int64_t *phigh,
                              int64_t divisor)
{
    __int128_t dividend = ((__int128_t)*phigh << 64) | *plow;
    __int128_t result = dividend / divisor;

    *plow = result;
    *phigh = result >> 64;
    return dividend % divisor;
}
#else
void muls64(uint64_t *plow, uint64_t *phigh, int64_t a, int64_t b);
void mulu64(uint64_t *plow, uint64_t *phigh, uint64_t a, uint64_t b);
uint64_t divu128(uint64_t *plow, uint64_t *phigh, uint64_t divisor);
int64_t divs128(uint64_t *plow, int64_t *phigh, int64_t divisor);

static inline uint64_t muldiv64_rounding(uint64_t a, uint32_t b, uint32_t c,
                                  bool round_up)
{
    union {
        uint64_t ll;
        struct {
#if HOST_BIG_ENDIAN
            uint32_t high, low;
#else
            uint32_t low, high;
#endif
        } l;
    } u, res;
    uint64_t rl, rh;

    u.ll = a;
    rl = (uint64_t)u.l.low * (uint64_t)b;
    if (round_up) {
        rl += c - 1;
    }
    rh = (uint64_t)u.l.high * (uint64_t)b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;
}

static inline uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    return muldiv64_rounding(a, b, c, false);
}

static inline uint64_t muldiv64_round_up(uint64_t a, uint32_t b, uint32_t c)
{
    return muldiv64_rounding(a, b, c, true);
}
#endif

static inline int clz8(uint8_t val)
{
    return val ? __builtin_clz(val) - 24 : 8;
}

static inline int clz16(uint16_t val)
{
    return val ? __builtin_clz(val) - 16 : 16;
}

static inline int clz32(uint32_t val)
{
    return val ? __builtin_clz(val) : 32;
}

static inline int clo32(uint32_t val)
{
    return clz32(~val);
}

static inline int clz64(uint64_t val)
{
    return val ? __builtin_clzll(val) : 64;
}

static inline int clo64(uint64_t val)
{
    return clz64(~val);
}

static inline int ctz8(uint8_t val)
{
    return val ? __builtin_ctz(val) : 8;
}

static inline int ctz16(uint16_t val)
{
    return val ? __builtin_ctz(val) : 16;
}

static inline int ctz32(uint32_t val)
{
    return val ? __builtin_ctz(val) : 32;
}

static inline int cto32(uint32_t val)
{
    return ctz32(~val);
}

static inline int ctz64(uint64_t val)
{
    return val ? __builtin_ctzll(val) : 64;
}

static inline int cto64(uint64_t val)
{
    return ctz64(~val);
}

static inline int clrsb32(uint32_t val)
{
#if __has_builtin(__builtin_clrsb) || !defined(__clang__)
    return __builtin_clrsb(val);
#else
    return clz32(val ^ ((int32_t)val >> 1)) - 1;
#endif
}

static inline int clrsb64(uint64_t val)
{
#if __has_builtin(__builtin_clrsbll) || !defined(__clang__)
    return __builtin_clrsbll(val);
#else
    return clz64(val ^ ((int64_t)val >> 1)) - 1;
#endif
}

static inline int ctpop8(uint8_t val)
{
    return __builtin_popcount(val);
}

static inline int parity8(uint8_t val)
{
    return __builtin_parity(val);
}

static inline int ctpop16(uint16_t val)
{
    return __builtin_popcount(val);
}

static inline int ctpop32(uint32_t val)
{
    return __builtin_popcount(val);
}

static inline int ctpop64(uint64_t val)
{
    return __builtin_popcountll(val);
}

static inline uint8_t revbit8(uint8_t x)
{
#if __has_builtin(__builtin_bitreverse8)
    return __builtin_bitreverse8(x);
#else
    x = ((x & 0xf0) >> 4)
      | ((x & 0x0f) << 4);
    x = ((x & 0x88) >> 3)
      | ((x & 0x44) >> 1)
      | ((x & 0x22) << 1)
      | ((x & 0x11) << 3);
    return x;
#endif
}

static inline uint16_t revbit16(uint16_t x)
{
#if __has_builtin(__builtin_bitreverse16)
    return __builtin_bitreverse16(x);
#else
    x = bswap16(x);
    x = ((x & 0xf0f0) >> 4)
      | ((x & 0x0f0f) << 4);
    x = ((x & 0x8888) >> 3)
      | ((x & 0x4444) >> 1)
      | ((x & 0x2222) << 1)
      | ((x & 0x1111) << 3);
    return x;
#endif
}

static inline uint32_t revbit32(uint32_t x)
{
#if __has_builtin(__builtin_bitreverse32)
    return __builtin_bitreverse32(x);
#else
    x = bswap32(x);
    x = ((x & 0xf0f0f0f0u) >> 4)
      | ((x & 0x0f0f0f0fu) << 4);
    x = ((x & 0x88888888u) >> 3)
      | ((x & 0x44444444u) >> 1)
      | ((x & 0x22222222u) << 1)
      | ((x & 0x11111111u) << 3);
    return x;
#endif
}

static inline uint64_t revbit64(uint64_t x)
{
#if __has_builtin(__builtin_bitreverse64)
    return __builtin_bitreverse64(x);
#else
    x = bswap64(x);
    x = ((x & 0xf0f0f0f0f0f0f0f0ull) >> 4)
      | ((x & 0x0f0f0f0f0f0f0f0full) << 4);
    x = ((x & 0x8888888888888888ull) >> 3)
      | ((x & 0x4444444444444444ull) >> 1)
      | ((x & 0x2222222222222222ull) << 1)
      | ((x & 0x1111111111111111ull) << 3);
    return x;
#endif
}

static inline uint64_t uabs64(int64_t v)
{
    return v < 0 ? -v : v;
}

static inline bool sadd32_overflow(int32_t x, int32_t y, int32_t *ret)
{
    return __builtin_add_overflow(x, y, ret);
}

static inline bool sadd64_overflow(int64_t x, int64_t y, int64_t *ret)
{
    return __builtin_add_overflow(x, y, ret);
}

static inline bool uadd32_overflow(uint32_t x, uint32_t y, uint32_t *ret)
{
    return __builtin_add_overflow(x, y, ret);
}

static inline bool uadd64_overflow(uint64_t x, uint64_t y, uint64_t *ret)
{
    return __builtin_add_overflow(x, y, ret);
}

static inline bool ssub32_overflow(int32_t x, int32_t y, int32_t *ret)
{
    return __builtin_sub_overflow(x, y, ret);
}

static inline bool ssub64_overflow(int64_t x, int64_t y, int64_t *ret)
{
    return __builtin_sub_overflow(x, y, ret);
}

static inline bool usub32_overflow(uint32_t x, uint32_t y, uint32_t *ret)
{
    return __builtin_sub_overflow(x, y, ret);
}

static inline bool usub64_overflow(uint64_t x, uint64_t y, uint64_t *ret)
{
    return __builtin_sub_overflow(x, y, ret);
}

static inline bool smul32_overflow(int32_t x, int32_t y, int32_t *ret)
{
    return __builtin_mul_overflow(x, y, ret);
}

static inline bool smul64_overflow(int64_t x, int64_t y, int64_t *ret)
{
    return __builtin_mul_overflow(x, y, ret);
}

static inline bool umul32_overflow(uint32_t x, uint32_t y, uint32_t *ret)
{
    return __builtin_mul_overflow(x, y, ret);
}

static inline bool umul64_overflow(uint64_t x, uint64_t y, uint64_t *ret)
{
    return __builtin_mul_overflow(x, y, ret);
}

static inline bool mulu128(uint64_t *plow, uint64_t *phigh, uint64_t factor)
{
#if defined(CONFIG_INT128)
    bool res;
    __uint128_t r;
    __uint128_t f = ((__uint128_t)*phigh << 64) | *plow;
    res = __builtin_mul_overflow(f, factor, &r);

    *plow = r;
    *phigh = r >> 64;

    return res;
#else
    uint64_t dhi = *phigh;
    uint64_t dlo = *plow;
    uint64_t ahi;
    uint64_t blo, bhi;

    if (dhi == 0) {
        mulu64(plow, phigh, dlo, factor);
        return false;
    }

    mulu64(plow, &ahi, dlo, factor);
    mulu64(&blo, &bhi, dhi, factor);

    return uadd64_overflow(ahi, blo, phigh) || bhi != 0;
#endif
}

static inline uint64_t uadd64_carry(uint64_t x, uint64_t y, bool *pcarry)
{
#if __has_builtin(__builtin_addcll)
    unsigned long long c = *pcarry;
    x = __builtin_addcll(x, y, c, &c);
    *pcarry = c & 1;
    return x;
#else
    bool c = *pcarry;
    c = uadd64_overflow(x, c, &x);
    c |= uadd64_overflow(x, y, &x);
    *pcarry = c;
    return x;
#endif
}

static inline uint64_t usub64_borrow(uint64_t x, uint64_t y, bool *pborrow)
{
#if __has_builtin(__builtin_subcll) && !defined(BUILTIN_SUBCLL_BROKEN)
    unsigned long long b = *pborrow;
    x = __builtin_subcll(x, y, b, &b);
    *pborrow = b & 1;
    return x;
#else
    bool b = *pborrow;
    b = usub64_overflow(x, b, &x);
    b |= usub64_overflow(x, y, &x);
    *pborrow = b;
    return x;
#endif
}


#if ULONG_MAX == UINT32_MAX
# define clzl   clz32
# define ctzl   ctz32
# define clol   clo32
# define ctol   cto32
# define ctpopl ctpop32
# define revbitl revbit32
#elif ULONG_MAX == UINT64_MAX
# define clzl   clz64
# define ctzl   ctz64
# define clol   clo64
# define ctol   cto64
# define ctpopl ctpop64
# define revbitl revbit64
#else
# error Unknown sizeof long
#endif

static inline bool is_power_of_2(uint64_t value)
{
    if (!value) {
        return false;
    }

    return !(value & (value - 1));
}

static inline uint64_t pow2floor(uint64_t value)
{
    if (!value) {
        return 0;
    }
    return 0x8000000000000000ull >> clz64(value);
}

static inline uint64_t pow2ceil(uint64_t value)
{
    int n = clz64(value - 1);

    if (!n) {
        return !value;
    }
    return 0x8000000000000000ull >> (n - 1);
}

static inline uint32_t pow2roundup32(uint32_t x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return x + 1;
}

void urshift(uint64_t *plow, uint64_t *phigh, int32_t shift);

void ulshift(uint64_t *plow, uint64_t *phigh, int32_t shift, bool *overflow);

static inline uint64_t udiv_qrnnd(uint64_t *r, uint64_t n1,
                                  uint64_t n0, uint64_t d)
{
#if defined(__x86_64__)
    uint64_t q;
    asm("divq %4" : "=a"(q), "=d"(*r) : "0"(n0), "1"(n1), "rm"(d));
    return q;
#elif defined(__s390x__) && !defined(__clang__)
    unsigned __int128 n = (unsigned __int128)n1 << 64 | n0;
    asm("dlgr %0, %1" : "+r"(n) : "r"(d));
    *r = n >> 64;
    return n;
#elif defined(_ARCH_PPC64) && defined(_ARCH_PWR7)
    uint64_t q1, q2, Q, r1, r2, R;
    asm("divdeu %0,%2,%4; divdu %1,%3,%4"
        : "=&r"(q1), "=r"(q2)
        : "r"(n1), "r"(n0), "r"(d));
    r1 = -(q1 * d);         /* low part of (n1<<64) - (q1 * d) */
    r2 = n0 - (q2 * d);
    Q = q1 + q2;
    R = r1 + r2;
    if (R >= d || R < r2) { /* overflow implies R > d */
        Q += 1;
        R -= d;
    }
    *r = R;
    return Q;
#else
    uint64_t d0, d1, q0, q1, r1, r0, m;

    d0 = (uint32_t)d;
    d1 = d >> 32;

    r1 = n1 % d1;
    q1 = n1 / d1;
    m = q1 * d0;
    r1 = (r1 << 32) | (n0 >> 32);
    if (r1 < m) {
        q1 -= 1;
        r1 += d;
        if (r1 >= d) {
            if (r1 < m) {
                q1 -= 1;
                r1 += d;
            }
        }
    }
    r1 -= m;

    r0 = r1 % d1;
    q0 = r1 / d1;
    m = q0 * d0;
    r0 = (r0 << 32) | (uint32_t)n0;
    if (r0 < m) {
        q0 -= 1;
        r0 += d;
        if (r0 >= d) {
            if (r0 < m) {
                q0 -= 1;
                r0 += d;
            }
        }
    }
    r0 -= m;

    *r = r0;
    return (q1 << 32) | q0;
#endif
}

Int128 divu256(Int128 *plow, Int128 *phigh, Int128 divisor);
Int128 divs256(Int128 *plow, Int128 *phigh, Int128 divisor);
#endif
