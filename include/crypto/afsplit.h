
#ifndef QCRYPTO_AFSPLIT_H
#define QCRYPTO_AFSPLIT_H

#include "crypto/hash.h"


int qcrypto_afsplit_encode(QCryptoHashAlgo hash,
                           size_t blocklen,
                           uint32_t stripes,
                           const uint8_t *in,
                           uint8_t *out,
                           Error **errp);

int qcrypto_afsplit_decode(QCryptoHashAlgo hash,
                           size_t blocklen,
                           uint32_t stripes,
                           const uint8_t *in,
                           uint8_t *out,
                           Error **errp);

#endif /* QCRYPTO_AFSPLIT_H */
