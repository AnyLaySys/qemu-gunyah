
#ifndef QCRYPTO_X509_UTILS_H
#define QCRYPTO_X509_UTILS_H

#include "crypto/hash.h"

int qcrypto_get_x509_cert_fingerprint(uint8_t *cert, size_t size,
                                      QCryptoHashAlgo hash,
                                      uint8_t *result,
                                      size_t *resultlen,
                                      Error **errp);

#endif
