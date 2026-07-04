
#ifndef QCRYPTO_HMAC_H
#define QCRYPTO_HMAC_H

#include "qapi/qapi-types-crypto.h"

typedef struct QCryptoHmac QCryptoHmac;
struct QCryptoHmac {
    QCryptoHashAlgo alg;
    void *opaque;
    void *driver;
};

bool qcrypto_hmac_supports(QCryptoHashAlgo alg);

QCryptoHmac *qcrypto_hmac_new(QCryptoHashAlgo alg,
                              const uint8_t *key, size_t nkey,
                              Error **errp);

void qcrypto_hmac_free(QCryptoHmac *hmac);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoHmac, qcrypto_hmac_free)

int qcrypto_hmac_bytesv(QCryptoHmac *hmac,
                        const struct iovec *iov,
                        size_t niov,
                        uint8_t **result,
                        size_t *resultlen,
                        Error **errp);

int qcrypto_hmac_bytes(QCryptoHmac *hmac,
                       const char *buf,
                       size_t len,
                       uint8_t **result,
                       size_t *resultlen,
                       Error **errp);

int qcrypto_hmac_digestv(QCryptoHmac *hmac,
                         const struct iovec *iov,
                         size_t niov,
                         char **digest,
                         Error **errp);

int qcrypto_hmac_digest(QCryptoHmac *hmac,
                        const char *buf,
                        size_t len,
                        char **digest,
                        Error **errp);

#endif
