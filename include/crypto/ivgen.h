
#ifndef QCRYPTO_IVGEN_H
#define QCRYPTO_IVGEN_H

#include "crypto/cipher.h"
#include "crypto/hash.h"


typedef struct QCryptoIVGen QCryptoIVGen;



QCryptoIVGen *qcrypto_ivgen_new(QCryptoIVGenAlgo alg,
                                QCryptoCipherAlgo cipheralg,
                                QCryptoHashAlgo hash,
                                const uint8_t *key, size_t nkey,
                                Error **errp);

int qcrypto_ivgen_calculate(QCryptoIVGen *ivgen,
                            uint64_t sector,
                            uint8_t *iv, size_t niv,
                            Error **errp);


QCryptoIVGenAlgo qcrypto_ivgen_get_algorithm(QCryptoIVGen *ivgen);


QCryptoCipherAlgo qcrypto_ivgen_get_cipher(QCryptoIVGen *ivgen);


QCryptoHashAlgo qcrypto_ivgen_get_hash(QCryptoIVGen *ivgen);


void qcrypto_ivgen_free(QCryptoIVGen *ivgen);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoIVGen, qcrypto_ivgen_free)

#endif /* QCRYPTO_IVGEN_H */
