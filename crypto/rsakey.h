
#ifndef QCRYPTO_RSAKEY_H
#define QCRYPTO_RSAKEY_H

#include "qemu/host-utils.h"
#include "crypto/akcipher.h"

typedef struct QCryptoAkCipherRSAKey QCryptoAkCipherRSAKey;
typedef struct QCryptoAkCipherMPI QCryptoAkCipherMPI;

struct QCryptoAkCipherMPI {
    uint8_t *data;
    size_t len;
};

struct QCryptoAkCipherRSAKey {
    QCryptoAkCipherMPI n;
    QCryptoAkCipherMPI e;
    QCryptoAkCipherMPI d;
    QCryptoAkCipherMPI p;
    QCryptoAkCipherMPI q;
    QCryptoAkCipherMPI dp;
    QCryptoAkCipherMPI dq;
    QCryptoAkCipherMPI u;
};

QCryptoAkCipherRSAKey *qcrypto_akcipher_rsakey_parse(
    QCryptoAkCipherKeyType type,
    const uint8_t *key, size_t keylen, Error **errp);

void qcrypto_akcipher_rsakey_export_p8info(const uint8_t *key,
                                           size_t keylen,
                                           uint8_t **dst,
                                           size_t *dlen);

void qcrypto_akcipher_rsakey_free(QCryptoAkCipherRSAKey *key);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoAkCipherRSAKey,
                              qcrypto_akcipher_rsakey_free);

#endif
