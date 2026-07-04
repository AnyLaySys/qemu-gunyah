
#ifndef QCRYPTO_CIPHER_H
#define QCRYPTO_CIPHER_H

#include "qapi/qapi-types-crypto.h"

typedef struct QCryptoCipher QCryptoCipher;
typedef struct QCryptoCipherDriver QCryptoCipherDriver;



struct QCryptoCipher {
    QCryptoCipherAlgo alg;
    QCryptoCipherMode mode;
    const QCryptoCipherDriver *driver;
};

bool qcrypto_cipher_supports(QCryptoCipherAlgo alg,
                             QCryptoCipherMode mode);

size_t qcrypto_cipher_get_block_len(QCryptoCipherAlgo alg);


size_t qcrypto_cipher_get_key_len(QCryptoCipherAlgo alg);


size_t qcrypto_cipher_get_iv_len(QCryptoCipherAlgo alg,
                                 QCryptoCipherMode mode);


QCryptoCipher *qcrypto_cipher_new(QCryptoCipherAlgo alg,
                                  QCryptoCipherMode mode,
                                  const uint8_t *key, size_t nkey,
                                  Error **errp);

void qcrypto_cipher_free(QCryptoCipher *cipher);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoCipher, qcrypto_cipher_free)

int qcrypto_cipher_encrypt(QCryptoCipher *cipher,
                           const void *in,
                           void *out,
                           size_t len,
                           Error **errp);


int qcrypto_cipher_decrypt(QCryptoCipher *cipher,
                           const void *in,
                           void *out,
                           size_t len,
                           Error **errp);

int qcrypto_cipher_setiv(QCryptoCipher *cipher,
                         const uint8_t *iv, size_t niv,
                         Error **errp);

#endif /* QCRYPTO_CIPHER_H */
