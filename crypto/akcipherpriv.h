
#ifndef QCRYPTO_AKCIPHERPRIV_H
#define QCRYPTO_AKCIPHERPRIV_H

#include "qapi/qapi-types-crypto.h"

typedef struct QCryptoAkCipherDriver QCryptoAkCipherDriver;

struct QCryptoAkCipher {
    QCryptoAkCipherAlgo alg;
    QCryptoAkCipherKeyType type;
    int max_plaintext_len;
    int max_ciphertext_len;
    int max_signature_len;
    int max_dgst_len;
    QCryptoAkCipherDriver *driver;
};

struct QCryptoAkCipherDriver {
    int (*encrypt)(QCryptoAkCipher *akcipher,
                   const void *in, size_t in_len,
                   void *out, size_t out_len, Error **errp);
    int (*decrypt)(QCryptoAkCipher *akcipher,
                   const void *out, size_t out_len,
                   void *in, size_t in_len, Error **errp);
    int (*sign)(QCryptoAkCipher *akcipher,
                const void *in, size_t in_len,
                void *out, size_t out_len, Error **errp);
    int (*verify)(QCryptoAkCipher *akcipher,
                  const void *in, size_t in_len,
                  const void *in2, size_t in2_len, Error **errp);
    void (*free)(QCryptoAkCipher *akcipher);
};

#endif /* QCRYPTO_AKCIPHER_H */
