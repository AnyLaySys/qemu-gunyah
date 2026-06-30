#ifndef QEMU_NET_TAP_H
#define QEMU_NET_TAP_H

#include "net/net.h"

int tap_enable(NetClientState *nc);
int tap_disable(NetClientState *nc);
int tap_get_fd(NetClientState *nc);

#endif
