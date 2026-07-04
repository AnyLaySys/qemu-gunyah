
#ifndef QEMU_SOCKETS_H
#define QEMU_SOCKETS_H

#ifdef _WIN32

int inet_aton(const char *cp, struct in_addr *ia);

#endif /* !_WIN32 */

#include "qapi/qapi-types-sockets.h"

bool fd_is_socket(int fd);
int qemu_socket(int domain, int type, int protocol);

int qemu_socketpair(int domain, int type, int protocol, int sv[2]);

int qemu_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
ssize_t qemu_send_full(int s, const void *buf, size_t count)
    G_GNUC_WARN_UNUSED_RESULT;
int socket_set_cork(int fd, int v);
int socket_set_nodelay(int fd);
void qemu_socket_set_block(int fd);
int qemu_socket_try_set_nonblock(int fd);
void qemu_socket_set_nonblock(int fd);
int socket_set_fast_reuse(int fd);

#ifdef WIN32
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2
#endif

int inet_ai_family_from_address(InetSocketAddress *addr,
                                Error **errp);
int inet_parse(InetSocketAddress *addr, const char *str, Error **errp);
int inet_connect_saddr(InetSocketAddress *saddr, Error **errp);

NetworkAddressFamily inet_netfamily(int family);

int unix_listen(const char *path, Error **errp);
int unix_connect(const char *path, Error **errp);

char *socket_uri(SocketAddress *addr);
SocketAddress *socket_parse(const char *str, Error **errp);
int socket_connect(SocketAddress *addr, Error **errp);
int socket_listen(SocketAddress *addr, int num, Error **errp);
void socket_listen_cleanup(int fd, Error **errp);
int socket_dgram(SocketAddress *remote, SocketAddress *local, Error **errp);

int convert_host_port(struct sockaddr_in *saddr, const char *host,
                      const char *port, Error **errp);
int parse_host_port(struct sockaddr_in *saddr, const char *str,
                    Error **errp);
int socket_init(void);

SocketAddress *
socket_sockaddr_to_address(struct sockaddr_storage *sa,
                           socklen_t salen,
                           Error **errp);

SocketAddress *socket_local_address(int fd, Error **errp);

SocketAddress *socket_address_flatten(SocketAddressLegacy *addr);

int socket_address_parse_named_fd(SocketAddress *addr, Error **errp);
#endif /* QEMU_SOCKETS_H */
