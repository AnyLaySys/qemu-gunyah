
#ifndef QCRYPTO_TLSCREDSPSK_H
#define QCRYPTO_TLSCREDSPSK_H

#include "crypto/tlscreds.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_TLS_CREDS_PSK "tls-creds-psk"
typedef struct QCryptoTLSCredsPSK QCryptoTLSCredsPSK;
DECLARE_INSTANCE_CHECKER(QCryptoTLSCredsPSK, QCRYPTO_TLS_CREDS_PSK,
                         TYPE_QCRYPTO_TLS_CREDS_PSK)

typedef struct QCryptoTLSCredsPSKClass QCryptoTLSCredsPSKClass;

#define QCRYPTO_TLS_CREDS_PSKFILE "keys.psk"


struct QCryptoTLSCredsPSKClass {
    QCryptoTLSCredsClass parent_class;
};


#endif /* QCRYPTO_TLSCREDSPSK_H */
