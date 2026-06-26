#ifndef QEMU_NET_CLIENTS_H
#define QEMU_NET_CLIENTS_H

#include "net/net.h"

#ifdef CONFIG_SLIRP
int net_init_slirp(const Netdev *netdev, const char *name,
                   NetClientState *peer, Error **errp);
#endif

#endif /* QEMU_NET_CLIENTS_H */
