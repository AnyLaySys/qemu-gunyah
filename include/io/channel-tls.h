
#ifndef QIO_CHANNEL_TLS_H
#define QIO_CHANNEL_TLS_H

#include "io/channel.h"
#include "io/task.h"
#include "crypto/tlssession.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_TLS "qio-channel-tls"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelTLS, QIO_CHANNEL_TLS)



struct QIOChannelTLS {
    QIOChannel parent;
    QIOChannel *master;
    QCryptoTLSSession *session;
    QIOChannelShutdown shutdown;
    guint hs_ioc_tag;
    guint bye_ioc_tag;
};

void qio_channel_tls_bye(QIOChannelTLS *ioc, Error **errp);

QIOChannelTLS *
qio_channel_tls_new_server(QIOChannel *master,
                           QCryptoTLSCreds *creds,
                           const char *aclname,
                           Error **errp);

QIOChannelTLS *
qio_channel_tls_new_client(QIOChannel *master,
                           QCryptoTLSCreds *creds,
                           const char *hostname,
                           Error **errp);

void qio_channel_tls_handshake(QIOChannelTLS *ioc,
                               QIOTaskFunc func,
                               gpointer opaque,
                               GDestroyNotify destroy,
                               GMainContext *context);

QCryptoTLSSession *
qio_channel_tls_get_session(QIOChannelTLS *ioc);

#endif /* QIO_CHANNEL_TLS_H */
