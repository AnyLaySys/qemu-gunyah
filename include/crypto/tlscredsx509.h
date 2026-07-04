
#ifndef QCRYPTO_TLSCREDSX509_H
#define QCRYPTO_TLSCREDSX509_H

#include "crypto/tlscreds.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_TLS_CREDS_X509 "tls-creds-x509"
typedef struct QCryptoTLSCredsX509 QCryptoTLSCredsX509;
DECLARE_INSTANCE_CHECKER(QCryptoTLSCredsX509, QCRYPTO_TLS_CREDS_X509,
                         TYPE_QCRYPTO_TLS_CREDS_X509)

typedef struct QCryptoTLSCredsX509Class QCryptoTLSCredsX509Class;

#define QCRYPTO_TLS_CREDS_X509_CA_CERT "ca-cert.pem"
#define QCRYPTO_TLS_CREDS_X509_CA_CRL "ca-crl.pem"
#define QCRYPTO_TLS_CREDS_X509_SERVER_KEY "server-key.pem"
#define QCRYPTO_TLS_CREDS_X509_SERVER_CERT "server-cert.pem"
#define QCRYPTO_TLS_CREDS_X509_CLIENT_KEY "client-key.pem"
#define QCRYPTO_TLS_CREDS_X509_CLIENT_CERT "client-cert.pem"



struct QCryptoTLSCredsX509Class {
    QCryptoTLSCredsClass parent_class;
};


#endif /* QCRYPTO_TLSCREDSX509_H */
