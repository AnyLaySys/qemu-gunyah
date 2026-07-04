


#ifndef FPU_SOFTFLOAT_MACROS_H
#define FPU_SOFTFLOAT_MACROS_H

#include "fpu/softfloat-types.h"
#include "qemu/host-utils.h"

static inline uint64_t shl_double(uint64_t l, uint64_t r, int c)
{
#if defined(__x86_64__)
    asm("shld %b2, %1, %0" : "+r"(l) : "r"(r), "ci"(c));
    return l;
#else
    return c ? (l << c) | (r >> (64 - c)) : l;
#endif
}

static inline uint64_t shr_double(uint64_t l, uint64_t r, int c)
{
#if defined(__x86_64__)
    asm("shrd %b2, %1, %0" : "+r"(r) : "r"(l), "ci"(c));
    return r;
#else
    return c ? (r >> c) | (l << (64 - c)) : r;
#endif
}


static inline void shift32RightJamming(uint32_t a, int count, uint32_t *zPtr)
{
    uint32_t z;

    if ( count == 0 ) {
        z = a;
    }
    else if ( count < 32 ) {
        z = ( a>>count ) | ( ( a<<( ( - count ) & 31 ) ) != 0 );
    }
    else {
        z = ( a != 0 );
    }
    *zPtr = z;

}


static inline void shift64RightJamming(uint64_t a, int count, uint64_t *zPtr)
{
    uint64_t z;

    if ( count == 0 ) {
        z = a;
    }
    else if ( count < 64 ) {
        z = ( a>>count ) | ( ( a<<( ( - count ) & 63 ) ) != 0 );
    }
    else {
        z = ( a != 0 );
    }
    *zPtr = z;

}


static inline void
 shift64ExtraRightJamming(
     uint64_t a0, uint64_t a1, int count, uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    uint64_t z0, z1;
    int8_t negCount = ( - count ) & 63;

    if ( count == 0 ) {
        z1 = a1;
        z0 = a0;
    }
    else if ( count < 64 ) {
        z1 = ( a0<<negCount ) | ( a1 != 0 );
        z0 = a0>>count;
    }
    else {
        if ( count == 64 ) {
            z1 = a0 | ( a1 != 0 );
        }
        else {
            z1 = ( ( a0 | a1 ) != 0 );
        }
        z0 = 0;
    }
    *z1Ptr = z1;
    *z0Ptr = z0;

}


static inline void
 shift128Right(
     uint64_t a0, uint64_t a1, int count, uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    uint64_t z0, z1;
    int8_t negCount = ( - count ) & 63;

    if ( count == 0 ) {
        z1 = a1;
        z0 = a0;
    }
    else if ( count < 64 ) {
        z1 = ( a0<<negCount ) | ( a1>>count );
        z0 = a0>>count;
    }
    else {
        z1 = (count < 128) ? (a0 >> (count & 63)) : 0;
        z0 = 0;
    }
    *z1Ptr = z1;
    *z0Ptr = z0;

}


static inline void
 shift128RightJamming(
     uint64_t a0, uint64_t a1, int count, uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    uint64_t z0, z1;
    int8_t negCount = ( - count ) & 63;

    if ( count == 0 ) {
        z1 = a1;
        z0 = a0;
    }
    else if ( count < 64 ) {
        z1 = ( a0<<negCount ) | ( a1>>count ) | ( ( a1<<negCount ) != 0 );
        z0 = a0>>count;
    }
    else {
        if ( count == 64 ) {
            z1 = a0 | ( a1 != 0 );
        }
        else if ( count < 128 ) {
            z1 = ( a0>>( count & 63 ) ) | ( ( ( a0<<negCount ) | a1 ) != 0 );
        }
        else {
            z1 = ( ( a0 | a1 ) != 0 );
        }
        z0 = 0;
    }
    *z1Ptr = z1;
    *z0Ptr = z0;

}


static inline void
 shift128ExtraRightJamming(
     uint64_t a0,
     uint64_t a1,
     uint64_t a2,
     int count,
     uint64_t *z0Ptr,
     uint64_t *z1Ptr,
     uint64_t *z2Ptr
 )
{
    uint64_t z0, z1, z2;
    int8_t negCount = ( - count ) & 63;

    if ( count == 0 ) {
        z2 = a2;
        z1 = a1;
        z0 = a0;
    }
    else {
        if ( count < 64 ) {
            z2 = a1<<negCount;
            z1 = ( a0<<negCount ) | ( a1>>count );
            z0 = a0>>count;
        }
        else {
            if ( count == 64 ) {
                z2 = a1;
                z1 = a0;
            }
            else {
                a2 |= a1;
                if ( count < 128 ) {
                    z2 = a0<<negCount;
                    z1 = a0>>( count & 63 );
                }
                else {
                    z2 = ( count == 128 ) ? a0 : ( a0 != 0 );
                    z1 = 0;
                }
            }
            z0 = 0;
        }
        z2 |= ( a2 != 0 );
    }
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}


static inline void shortShift128Left(uint64_t a0, uint64_t a1, int count,
                                     uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    *z1Ptr = a1 << count;
    *z0Ptr = count == 0 ? a0 : (a0 << count) | (a1 >> (-count & 63));
}


static inline void shift128Left(uint64_t a0, uint64_t a1, int count,
                                uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    if (count < 64) {
        *z1Ptr = a1 << count;
        *z0Ptr = count == 0 ? a0 : (a0 << count) | (a1 >> (-count & 63));
    } else {
        *z1Ptr = 0;
        *z0Ptr = a1 << (count - 64);
    }
}


static inline void
 shortShift192Left(
     uint64_t a0,
     uint64_t a1,
     uint64_t a2,
     int count,
     uint64_t *z0Ptr,
     uint64_t *z1Ptr,
     uint64_t *z2Ptr
 )
{
    uint64_t z0, z1, z2;
    int8_t negCount;

    z2 = a2<<count;
    z1 = a1<<count;
    z0 = a0<<count;
    if ( 0 < count ) {
        negCount = ( ( - count ) & 63 );
        z1 |= a2>>negCount;
        z0 |= a1>>negCount;
    }
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}


static inline void add128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1,
                          uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    bool c = 0;
    *z1Ptr = uadd64_carry(a1, b1, &c);
    *z0Ptr = uadd64_carry(a0, b0, &c);
}


static inline void add192(uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t b0, uint64_t b1, uint64_t b2,
                          uint64_t *z0Ptr, uint64_t *z1Ptr, uint64_t *z2Ptr)
{
    bool c = 0;
    *z2Ptr = uadd64_carry(a2, b2, &c);
    *z1Ptr = uadd64_carry(a1, b1, &c);
    *z0Ptr = uadd64_carry(a0, b0, &c);
}


static inline void sub128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1,
                          uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    bool c = 0;
    *z1Ptr = usub64_borrow(a1, b1, &c);
    *z0Ptr = usub64_borrow(a0, b0, &c);
}


static inline void sub192(uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t b0, uint64_t b1, uint64_t b2,
                          uint64_t *z0Ptr, uint64_t *z1Ptr, uint64_t *z2Ptr)
{
    bool c = 0;
    *z2Ptr = usub64_borrow(a2, b2, &c);
    *z1Ptr = usub64_borrow(a1, b1, &c);
    *z0Ptr = usub64_borrow(a0, b0, &c);
}


static inline void
mul64To128(uint64_t a, uint64_t b, uint64_t *z0Ptr, uint64_t *z1Ptr)
{
    mulu64(z1Ptr, z0Ptr, a, b);
}


static inline void
mul128By64To192(uint64_t a0, uint64_t a1, uint64_t b,
                uint64_t *z0Ptr, uint64_t *z1Ptr, uint64_t *z2Ptr)
{
    uint64_t z0, z1, m1;

    mul64To128(a1, b, &m1, z2Ptr);
    mul64To128(a0, b, &z0, &z1);
    add128(z0, z1, 0, m1, z0Ptr, z1Ptr);
}


static inline void mul128To256(uint64_t a0, uint64_t a1,
                               uint64_t b0, uint64_t b1,
                               uint64_t *z0Ptr, uint64_t *z1Ptr,
                               uint64_t *z2Ptr, uint64_t *z3Ptr)
{
    uint64_t z0, z1, z2;
    uint64_t m0, m1, m2, n1, n2;

    mul64To128(a1, b0, &m1, &m2);
    mul64To128(a0, b1, &n1, &n2);
    mul64To128(a1, b1, &z2, z3Ptr);
    mul64To128(a0, b0, &z0, &z1);

    add192( 0, m1, m2,  0, n1, n2, &m0, &m1, &m2);
    add192(m0, m1, m2, z0, z1, z2, z0Ptr, z1Ptr, z2Ptr);
}


static inline uint64_t estimateDiv128To64(uint64_t a0, uint64_t a1, uint64_t b)
{
    uint64_t b0, b1;
    uint64_t rem0, rem1, term0, term1;
    uint64_t z;

    if ( b <= a0 ) return UINT64_C(0xFFFFFFFFFFFFFFFF);
    b0 = b>>32;
    z = ( b0<<32 <= a0 ) ? UINT64_C(0xFFFFFFFF00000000) : ( a0 / b0 )<<32;
    mul64To128( b, z, &term0, &term1 );
    sub128( a0, a1, term0, term1, &rem0, &rem1 );
    while ( ( (int64_t) rem0 ) < 0 ) {
        z -= UINT64_C(0x100000000);
        b1 = b<<32;
        add128( rem0, rem1, b0, b1, &rem0, &rem1 );
    }
    rem0 = ( rem0<<32 ) | ( rem1>>32 );
    z |= ( b0<<32 <= rem0 ) ? 0xFFFFFFFF : rem0 / b0;
    return z;

}


static inline uint32_t estimateSqrt32(int aExp, uint32_t a)
{
    static const uint16_t sqrtOddAdjustments[] = {
        0x0004, 0x0022, 0x005D, 0x00B1, 0x011D, 0x019F, 0x0236, 0x02E0,
        0x039C, 0x0468, 0x0545, 0x0631, 0x072B, 0x0832, 0x0946, 0x0A67
    };
    static const uint16_t sqrtEvenAdjustments[] = {
        0x0A2D, 0x08AF, 0x075A, 0x0629, 0x051A, 0x0429, 0x0356, 0x029E,
        0x0200, 0x0179, 0x0109, 0x00AF, 0x0068, 0x0034, 0x0012, 0x0002
    };
    int8_t index;
    uint32_t z;

    index = ( a>>27 ) & 15;
    if ( aExp & 1 ) {
        z = 0x4000 + ( a>>17 ) - sqrtOddAdjustments[ (int)index ];
        z = ( ( a / z )<<14 ) + ( z<<15 );
        a >>= 1;
    }
    else {
        z = 0x8000 + ( a>>17 ) - sqrtEvenAdjustments[ (int)index ];
        z = a / z + z;
        z = ( 0x20000 <= z ) ? 0xFFFF8000 : ( z<<15 );
        if ( z <= a ) return (uint32_t) ( ( (int32_t) a )>>1 );
    }
    return ( (uint32_t) ( ( ( (uint64_t) a )<<31 ) / z ) ) + ( z>>1 );

}


static inline bool eq128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1)
{
    return a0 == b0 && a1 == b1;
}


static inline bool le128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1)
{
    return a0 < b0 || (a0 == b0 && a1 <= b1);
}


static inline bool lt128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1)
{
    return a0 < b0 || (a0 == b0 && a1 < b1);
}


static inline bool ne128(uint64_t a0, uint64_t a1, uint64_t b0, uint64_t b1)
{
    return a0 != b0 || a1 != b1;
}


static inline bool eq192(uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t b0, uint64_t b1, uint64_t b2)
{
    return ((a0 ^ b0) | (a1 ^ b1) | (a2 ^ b2)) == 0;
}

static inline bool le192(uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t b0, uint64_t b1, uint64_t b2)
{
    if (a0 != b0) {
        return a0 < b0;
    }
    if (a1 != b1) {
        return a1 < b1;
    }
    return a2 <= b2;
}

static inline bool lt192(uint64_t a0, uint64_t a1, uint64_t a2,
                         uint64_t b0, uint64_t b1, uint64_t b2)
{
    if (a0 != b0) {
        return a0 < b0;
    }
    if (a1 != b1) {
        return a1 < b1;
    }
    return a2 < b2;
}

#endif
