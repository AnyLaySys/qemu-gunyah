
#ifndef QCRYPTO_CIPHERPRIV_H
#define QCRYPTO_CIPHERPRIV_H

#include "qapi/qapi-types-crypto.h"

struct QCryptoCipherDriver {
    int (*cipher_encrypt)(QCryptoCipher *cipher,
                          const void *in,
                          void *out,
                          size_t len,
                          Error **errp);

    int (*cipher_decrypt)(QCryptoCipher *cipher,
                          const void *in,
                          void *out,
                          size_t len,
                          Error **errp);

    int (*cipher_setiv)(QCryptoCipher *cipher,
                        const uint8_t *iv, size_t niv,
                        Error **errp);

    void (*cipher_free)(QCryptoCipher *cipher);
};

#ifdef CONFIG_AF_ALG

#include "afalgpriv.h"

extern QCryptoCipher *
qcrypto_afalg_cipher_ctx_new(QCryptoCipherAlgo alg,
                             QCryptoCipherMode mode,
                             const uint8_t *key,
                             size_t nkey, Error **errp);

#endif

#endif
