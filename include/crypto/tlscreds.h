
#ifndef QCRYPTO_TLSCREDS_H
#define QCRYPTO_TLSCREDS_H

#include "qapi/qapi-types-crypto.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_TLS_CREDS "tls-creds"
typedef struct QCryptoTLSCreds QCryptoTLSCreds;
typedef struct QCryptoTLSCredsClass QCryptoTLSCredsClass;
DECLARE_OBJ_CHECKERS(QCryptoTLSCreds, QCryptoTLSCredsClass, QCRYPTO_TLS_CREDS,
                     TYPE_QCRYPTO_TLS_CREDS)


#define QCRYPTO_TLS_CREDS_DH_PARAMS "dh-params.pem"


typedef bool (*CryptoTLSCredsReload)(QCryptoTLSCreds *, Error **);

struct QCryptoTLSCredsClass {
    ObjectClass parent_class;
    CryptoTLSCredsReload reload;
};

bool qcrypto_tls_creds_check_endpoint(QCryptoTLSCreds *creds,
                                      QCryptoTLSCredsEndpoint endpoint,
                                      Error **errp);

#endif /* QCRYPTO_TLSCREDS_H */
