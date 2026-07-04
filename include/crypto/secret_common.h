
#ifndef QCRYPTO_SECRET_COMMON_H
#define QCRYPTO_SECRET_COMMON_H

#include "qapi/qapi-types-crypto.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_SECRET_COMMON "secret_common"
OBJECT_DECLARE_TYPE(QCryptoSecretCommon, QCryptoSecretCommonClass,
                    QCRYPTO_SECRET_COMMON)


struct QCryptoSecretCommon {
    Object parent_obj;
    uint8_t *rawdata;
    size_t rawlen;
    QCryptoSecretFormat format;
    char *keyid;
    char *iv;
};


struct QCryptoSecretCommonClass {
    ObjectClass parent_class;
    void (*load_data)(QCryptoSecretCommon *secret,
                      uint8_t **output,
                      size_t *outputlen,
                      Error **errp);
};


int qcrypto_secret_lookup(const char *secretid,
                          uint8_t **data,
                          size_t *datalen,
                          Error **errp);
char *qcrypto_secret_lookup_as_utf8(const char *secretid, Error **errp);
char *qcrypto_secret_lookup_as_base64(const char *secretid, Error **errp);

#endif /* QCRYPTO_SECRET_COMMON_H */
