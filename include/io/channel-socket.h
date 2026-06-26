
#ifndef QIO_CHANNEL_SOCKET_H
#define QIO_CHANNEL_SOCKET_H

#include "io/channel.h"
#include "io/task.h"
#include "qemu/sockets.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_SOCKET "qio-channel-socket"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelSocket, QIO_CHANNEL_SOCKET)



struct QIOChannelSocket {
    QIOChannel parent;
    int fd;
    struct sockaddr_storage localAddr;
    socklen_t localAddrLen;
    struct sockaddr_storage remoteAddr;
    socklen_t remoteAddrLen;
    ssize_t zero_copy_queued;
    ssize_t zero_copy_sent;
};


QIOChannelSocket *
qio_channel_socket_new(void);

QIOChannelSocket *
qio_channel_socket_new_fd(int fd,
                          Error **errp);


int qio_channel_socket_connect_sync(QIOChannelSocket *ioc,
                                    SocketAddress *addr,
                                    Error **errp);

void qio_channel_socket_connect_async(QIOChannelSocket *ioc,
                                      SocketAddress *addr,
                                      QIOTaskFunc callback,
                                      gpointer opaque,
                                      GDestroyNotify destroy,
                                      GMainContext *context);


int qio_channel_socket_listen_sync(QIOChannelSocket *ioc,
                                   SocketAddress *addr,
                                   int num,
                                   Error **errp);

void qio_channel_socket_listen_async(QIOChannelSocket *ioc,
                                     SocketAddress *addr,
                                     int num,
                                     QIOTaskFunc callback,
                                     gpointer opaque,
                                     GDestroyNotify destroy,
                                     GMainContext *context);


int qio_channel_socket_dgram_sync(QIOChannelSocket *ioc,
                                  SocketAddress *localAddr,
                                  SocketAddress *remoteAddr,
                                  Error **errp);

void qio_channel_socket_dgram_async(QIOChannelSocket *ioc,
                                    SocketAddress *localAddr,
                                    SocketAddress *remoteAddr,
                                    QIOTaskFunc callback,
                                    gpointer opaque,
                                    GDestroyNotify destroy,
                                    GMainContext *context);


SocketAddress *
qio_channel_socket_get_local_address(QIOChannelSocket *ioc,
                                     Error **errp);

SocketAddress *
qio_channel_socket_get_remote_address(QIOChannelSocket *ioc,
                                      Error **errp);


QIOChannelSocket *
qio_channel_socket_accept(QIOChannelSocket *ioc,
                          Error **errp);


#endif /* QIO_CHANNEL_SOCKET_H */
