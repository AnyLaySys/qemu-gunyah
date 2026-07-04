
#ifndef QIO_NET_LISTENER_H
#define QIO_NET_LISTENER_H

#include "io/channel-socket.h"
#include "qom/object.h"

#define TYPE_QIO_NET_LISTENER "qio-net-listener"
OBJECT_DECLARE_SIMPLE_TYPE(QIONetListener,
                           QIO_NET_LISTENER)


typedef void (*QIONetListenerClientFunc)(QIONetListener *listener,
                                         QIOChannelSocket *sioc,
                                         gpointer data);

struct QIONetListener {
    Object parent;

    char *name;
    QIOChannelSocket **sioc;
    GSource **io_source;
    size_t nsioc;

    bool connected;

    QIONetListenerClientFunc io_func;
    gpointer io_data;
    GDestroyNotify io_notify;
};



QIONetListener *qio_net_listener_new(void);


void qio_net_listener_set_name(QIONetListener *listener,
                               const char *name);

int qio_net_listener_open_sync(QIONetListener *listener,
                               SocketAddress *addr,
                               int num,
                               Error **errp);

void qio_net_listener_add(QIONetListener *listener,
                          QIOChannelSocket *sioc);

void qio_net_listener_set_client_func_full(QIONetListener *listener,
                                           QIONetListenerClientFunc func,
                                           gpointer data,
                                           GDestroyNotify notify,
                                           GMainContext *context);

void qio_net_listener_set_client_func(QIONetListener *listener,
                                      QIONetListenerClientFunc func,
                                      gpointer data,
                                      GDestroyNotify notify);

QIOChannelSocket *qio_net_listener_wait_client(QIONetListener *listener);


void qio_net_listener_disconnect(QIONetListener *listener);


bool qio_net_listener_is_connected(QIONetListener *listener);

#endif /* QIO_NET_LISTENER_H */
