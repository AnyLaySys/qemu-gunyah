
#ifndef QCRYPTO_TLSCREDSANON_H
#define QCRYPTO_TLSCREDSANON_H

#include "crypto/tlscreds.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_TLS_CREDS_ANON "tls-creds-anon"
typedef struct QCryptoTLSCredsAnon QCryptoTLSCredsAnon;
DECLARE_INSTANCE_CHECKER(QCryptoTLSCredsAnon, QCRYPTO_TLS_CREDS_ANON,
                         TYPE_QCRYPTO_TLS_CREDS_ANON)


typedef struct QCryptoTLSCredsAnonClass QCryptoTLSCredsAnonClass;


struct QCryptoTLSCredsAnonClass {
    QCryptoTLSCredsClass parent_class;
};


#endif /* QCRYPTO_TLSCREDSANON_H */
