#ifndef QEMU_NET_CLIENTS_H
#define QEMU_NET_CLIENTS_H

#include "net/net.h"

int net_init_tap(const Netdev *netdev, const char *name,
                 NetClientState *peer, Error **errp);

#endif /* QEMU_NET_CLIENTS_H */
