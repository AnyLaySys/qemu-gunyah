
#ifndef QIO_DNS_RESOLVER_H
#define QIO_DNS_RESOLVER_H

#include "qapi/qapi-types-sockets.h"
#include "qom/object.h"
#include "io/task.h"

#define TYPE_QIO_DNS_RESOLVER "qio-dns-resolver"
OBJECT_DECLARE_SIMPLE_TYPE(QIODNSResolver,
                           QIO_DNS_RESOLVER)


struct QIODNSResolver {
    Object parent;
};



QIODNSResolver *qio_dns_resolver_get_instance(void);

int qio_dns_resolver_lookup_sync(QIODNSResolver *resolver,
                                 SocketAddress *addr,
                                 size_t *naddrs,
                                 SocketAddress ***addrs,
                                 Error **errp);

void qio_dns_resolver_lookup_async(QIODNSResolver *resolver,
                                   SocketAddress *addr,
                                   QIOTaskFunc func,
                                   gpointer opaque,
                                   GDestroyNotify notify);

void qio_dns_resolver_lookup_result(QIODNSResolver *resolver,
                                    QIOTask *task,
                                    size_t *naddrs,
                                    SocketAddress ***addrs);

#endif /* QIO_DNS_RESOLVER_H */
