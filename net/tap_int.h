#ifndef QEMU_TAP_INT_H
#define QEMU_TAP_INT_H

#include <net/if.h>
#include "net/net.h"

#define TAP_LEASES 16
#define TAP_DNS_MAX 64

typedef struct TAPState TAPState;

typedef struct TapDns {
    TAPState *s;
    int fd;
    uint8_t mac[6];
    uint32_t ip;
    uint16_t port, id, len;
    uint8_t query[512];
} TapDns;

struct TAPState {
    NetClientState nc;
    int fd;
    uint8_t buf[NET_BUFSIZE];
    bool r, w, vnet, ufo, uso, enabled;
    char ifname[IFNAMSIZ];
    uint32_t addr, mask;
    uint8_t mac[6];
    uint16_t ip_id;
    unsigned lease_next, dns_next;
    uint8_t leases[TAP_LEASES][6];
    TapDns dns[TAP_DNS_MAX];
};

void tap_service_init(TAPState *s, const char *ifname);
void tap_service_set_network(uint64_t network);
bool tap_service_input(TAPState *s, const struct iovec *iov, int iovcnt,
                       size_t *len);
void tap_service_cleanup(TAPState *s);

#endif
