
#ifndef QCRYPTO_SECRET_KEYRING_H
#define QCRYPTO_SECRET_KEYRING_H

#include "qapi/qapi-types-crypto.h"
#include "qom/object.h"
#include "crypto/secret_common.h"

#define TYPE_QCRYPTO_SECRET_KEYRING "secret_keyring"
OBJECT_DECLARE_SIMPLE_TYPE(QCryptoSecretKeyring,
                           QCRYPTO_SECRET_KEYRING)


struct QCryptoSecretKeyring {
    QCryptoSecretCommon parent;
    int32_t serial;
};



#endif /* QCRYPTO_SECRET_KEYRING_H */
