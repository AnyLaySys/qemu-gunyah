
#ifndef GENERIC_HOST_CRYPTO_CLMUL_H
#define GENERIC_HOST_CRYPTO_CLMUL_H

#define HAVE_CLMUL_ACCEL  false
#define ATTR_CLMUL_ACCEL

Int128 clmul_64_accel(uint64_t, uint64_t)
    QEMU_ERROR("unsupported accel");

#endif /* GENERIC_HOST_CRYPTO_CLMUL_H */
