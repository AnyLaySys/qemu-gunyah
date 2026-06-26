
#ifndef QCRYPTO_HASHPRIV_H
#define QCRYPTO_HASHPRIV_H

#include "crypto/hash.h"

typedef struct QCryptoHashDriver QCryptoHashDriver;

struct QCryptoHashDriver {
    QCryptoHash *(*hash_new)(QCryptoHashAlgo alg, Error **errp);
    int (*hash_update)(QCryptoHash *hash,
                       const struct iovec *iov,
                       size_t niov,
                       Error **errp);
    int (*hash_finalize)(QCryptoHash *hash,
                         uint8_t **result,
                         size_t *resultlen,
                         Error **errp);
    void (*hash_free)(QCryptoHash *hash);
};

extern QCryptoHashDriver qcrypto_hash_lib_driver;

#ifdef CONFIG_AF_ALG

#include "afalgpriv.h"

extern QCryptoHashDriver qcrypto_hash_afalg_driver;

#endif

#endif
