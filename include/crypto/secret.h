
#ifndef QCRYPTO_SECRET_H
#define QCRYPTO_SECRET_H

#include "qapi/qapi-types-crypto.h"
#include "qom/object.h"
#include "crypto/secret_common.h"

#define TYPE_QCRYPTO_SECRET "secret"
typedef struct QCryptoSecret QCryptoSecret;
DECLARE_INSTANCE_CHECKER(QCryptoSecret, QCRYPTO_SECRET,
                         TYPE_QCRYPTO_SECRET)

typedef struct QCryptoSecretClass QCryptoSecretClass;


struct QCryptoSecret {
    QCryptoSecretCommon parent_obj;
    char *data;
    char *file;
};


struct QCryptoSecretClass {
    QCryptoSecretCommonClass parent_class;
};

#endif /* QCRYPTO_SECRET_H */
