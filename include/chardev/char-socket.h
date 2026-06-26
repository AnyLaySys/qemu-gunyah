
#ifndef CHAR_SOCKET_H
#define CHAR_SOCKET_H

#include "io/channel-socket.h"
#include "io/channel-tls.h"
#include "io/net-listener.h"
#include "chardev/char.h"
#include "qom/object.h"

#define TCP_MAX_FDS 16

typedef struct {
    char buf[21];
    size_t buflen;
} TCPChardevTelnetInit;

typedef enum {
    TCP_CHARDEV_STATE_DISCONNECTED,
    TCP_CHARDEV_STATE_CONNECTING,
    TCP_CHARDEV_STATE_CONNECTED,
} TCPChardevState;

typedef ChardevClass SocketChardevClass;

struct SocketChardev {
    Chardev parent;
    QIOChannel *ioc; /* Client I/O channel */
    QIOChannelSocket *sioc; /* Client master channel */
    QIONetListener *listener;
    GSource *hup_source;
    QCryptoTLSCreds *tls_creds;
    char *tls_authz;
    TCPChardevState state;
    int max_size;
    int do_telnetopt;
    int do_nodelay;
    int *read_msgfds;
    size_t read_msgfds_num;
    int *write_msgfds;
    size_t write_msgfds_num;
    bool registered_yank;

    SocketAddress *addr;
    bool is_listen;
    bool is_telnet;
    bool is_tn3270;
    GSource *telnet_source;
    TCPChardevTelnetInit *telnet_init;

    bool is_websock;

    GSource *reconnect_timer;
    int64_t reconnect_time_ms;
    bool connect_err_reported;

    QIOTask *connect_task;
};
typedef struct SocketChardev SocketChardev;

DECLARE_INSTANCE_CHECKER(SocketChardev, SOCKET_CHARDEV,
                         TYPE_CHARDEV_SOCKET)

#endif /* CHAR_SOCKET_H */
