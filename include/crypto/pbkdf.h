
#ifndef QCRYPTO_PBKDF_H
#define QCRYPTO_PBKDF_H

#include "crypto/hash.h"


bool qcrypto_pbkdf2_supports(QCryptoHashAlgo hash);


int qcrypto_pbkdf2(QCryptoHashAlgo hash,
                   const uint8_t *key, size_t nkey,
                   const uint8_t *salt, size_t nsalt,
                   uint64_t iterations,
                   uint8_t *out, size_t nout,
                   Error **errp);

uint64_t qcrypto_pbkdf2_count_iters(QCryptoHashAlgo hash,
                                    const uint8_t *key, size_t nkey,
                                    const uint8_t *salt, size_t nsalt,
                                    size_t nout,
                                    Error **errp);

#endif /* QCRYPTO_PBKDF_H */
