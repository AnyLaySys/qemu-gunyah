
#ifndef QEMU_JHASH_H
#define QEMU_JHASH_H

#include "qemu/bitops.h"


#define __jhash_mix(a, b, c)                \
{                                           \
    a -= c;  a ^= rol32(c, 4);  c += b;     \
    b -= a;  b ^= rol32(a, 6);  a += c;     \
    c -= b;  c ^= rol32(b, 8);  b += a;     \
    a -= c;  a ^= rol32(c, 16); c += b;     \
    b -= a;  b ^= rol32(a, 19); a += c;     \
    c -= b;  c ^= rol32(b, 4);  b += a;     \
}

#define __jhash_final(a, b, c)  \
{                               \
    c ^= b; c -= rol32(b, 14);  \
    a ^= c; a -= rol32(c, 11);  \
    b ^= a; b -= rol32(a, 25);  \
    c ^= b; c -= rol32(b, 16);  \
    a ^= c; a -= rol32(c, 4);   \
    b ^= a; b -= rol32(a, 14);  \
    c ^= b; c -= rol32(b, 24);  \
}

#define JHASH_INITVAL           0xdeadbeef

#endif /* QEMU_JHASH_H */
