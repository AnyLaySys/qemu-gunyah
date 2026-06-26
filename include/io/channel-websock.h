
#ifndef QIO_CHANNEL_WEBSOCK_H
#define QIO_CHANNEL_WEBSOCK_H

#include "io/channel.h"
#include "qemu/buffer.h"
#include "io/task.h"
#include "qom/object.h"

#define TYPE_QIO_CHANNEL_WEBSOCK "qio-channel-websock"
OBJECT_DECLARE_SIMPLE_TYPE(QIOChannelWebsock, QIO_CHANNEL_WEBSOCK)

typedef union QIOChannelWebsockMask QIOChannelWebsockMask;

union QIOChannelWebsockMask {
    char c[4];
    uint32_t u;
};


struct QIOChannelWebsock {
    QIOChannel parent;
    QIOChannel *master;
    Buffer encinput;
    Buffer encoutput;
    Buffer rawinput;
    size_t payload_remain;
    size_t pong_remain;
    QIOChannelWebsockMask mask;
    guint io_tag;
    Error *io_err;
    gboolean io_eof;
    uint8_t opcode;
};

QIOChannelWebsock *
qio_channel_websock_new_server(QIOChannel *master);

void qio_channel_websock_handshake(QIOChannelWebsock *ioc,
                                   QIOTaskFunc func,
                                   gpointer opaque,
                                   GDestroyNotify destroy);

#endif /* QIO_CHANNEL_WEBSOCK_H */
