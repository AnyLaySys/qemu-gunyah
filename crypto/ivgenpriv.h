
#ifndef QCRYPTO_IVGENPRIV_H
#define QCRYPTO_IVGENPRIV_H

#include "crypto/ivgen.h"

typedef struct QCryptoIVGenDriver QCryptoIVGenDriver;

struct QCryptoIVGenDriver {
    int (*init)(QCryptoIVGen *ivgen,
                const uint8_t *key, size_t nkey,
                Error **errp);
    int (*calculate)(QCryptoIVGen *ivgen,
                     uint64_t sector,
                     uint8_t *iv, size_t niv,
                     Error **errp);
    void (*cleanup)(QCryptoIVGen *ivgen);
};

struct QCryptoIVGen {
    QCryptoIVGenDriver *driver;
    void *private;

    QCryptoIVGenAlgo algorithm;
    QCryptoCipherAlgo cipher;
    QCryptoHashAlgo hash;
};


#endif /* QCRYPTO_IVGENPRIV_H */
