
#ifndef QCRYPTO_HMACPRIV_H
#define QCRYPTO_HMACPRIV_H

typedef struct QCryptoHmacDriver QCryptoHmacDriver;

struct QCryptoHmacDriver {
    int (*hmac_bytesv)(QCryptoHmac *hmac,
                       const struct iovec *iov,
                       size_t niov,
                       uint8_t **result,
                       size_t *resultlen,
                       Error **errp);

    void (*hmac_free)(QCryptoHmac *hmac);
};

void *qcrypto_hmac_ctx_new(QCryptoHashAlgo alg,
                           const uint8_t *key, size_t nkey,
                           Error **errp);
extern QCryptoHmacDriver qcrypto_hmac_lib_driver;

#ifdef CONFIG_AF_ALG

#include "afalgpriv.h"

QCryptoAFAlgo *qcrypto_afalg_hmac_ctx_new(QCryptoHashAlgo alg,
                                         const uint8_t *key, size_t nkey,
                                         Error **errp);
extern QCryptoHmacDriver qcrypto_hmac_afalg_driver;

#endif

#endif
