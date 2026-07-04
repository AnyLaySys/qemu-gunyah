
#ifndef QCRYPTO_TLSSESSION_H
#define QCRYPTO_TLSSESSION_H

#include "crypto/tlscreds.h"


typedef struct QCryptoTLSSession QCryptoTLSSession;

#define QCRYPTO_TLS_SESSION_ERR_BLOCK -2

QCryptoTLSSession *qcrypto_tls_session_new(QCryptoTLSCreds *creds,
                                           const char *hostname,
                                           const char *aclname,
                                           QCryptoTLSCredsEndpoint endpoint,
                                           Error **errp);

void qcrypto_tls_session_free(QCryptoTLSSession *sess);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QCryptoTLSSession, qcrypto_tls_session_free)

int qcrypto_tls_session_check_credentials(QCryptoTLSSession *sess,
                                          Error **errp);

typedef ssize_t (*QCryptoTLSSessionWriteFunc)(const char *buf,
                                              size_t len,
                                              void *opaque,
                                              Error **errp);
typedef ssize_t (*QCryptoTLSSessionReadFunc)(char *buf,
                                             size_t len,
                                             void *opaque,
                                             Error **errp);

void qcrypto_tls_session_set_callbacks(QCryptoTLSSession *sess,
                                       QCryptoTLSSessionWriteFunc writeFunc,
                                       QCryptoTLSSessionReadFunc readFunc,
                                       void *opaque);

ssize_t qcrypto_tls_session_write(QCryptoTLSSession *sess,
                                  const char *buf,
                                  size_t len,
                                  Error **errp);

ssize_t qcrypto_tls_session_read(QCryptoTLSSession *sess,
                                 char *buf,
                                 size_t len,
                                 bool gracefulTermination,
                                 Error **errp);

size_t qcrypto_tls_session_check_pending(QCryptoTLSSession *sess);

int qcrypto_tls_session_handshake(QCryptoTLSSession *sess,
                                  Error **errp);

typedef enum {
    QCRYPTO_TLS_HANDSHAKE_COMPLETE,
    QCRYPTO_TLS_HANDSHAKE_SENDING,
    QCRYPTO_TLS_HANDSHAKE_RECVING,
} QCryptoTLSSessionHandshakeStatus;

typedef enum {
    QCRYPTO_TLS_BYE_COMPLETE,
    QCRYPTO_TLS_BYE_SENDING,
    QCRYPTO_TLS_BYE_RECVING,
} QCryptoTLSSessionByeStatus;

int
qcrypto_tls_session_bye(QCryptoTLSSession *session, Error **errp);

int qcrypto_tls_session_get_key_size(QCryptoTLSSession *sess,
                                     Error **errp);

char *qcrypto_tls_session_get_peer_name(QCryptoTLSSession *sess);

#endif /* QCRYPTO_TLSSESSION_H */
