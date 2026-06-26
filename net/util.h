
#ifndef QEMU_NET_UTIL_H
#define QEMU_NET_UTIL_H


struct ip {
#if HOST_BIG_ENDIAN
    uint8_t ip_v:4,         /* version */
            ip_hl:4;        /* header length */
#else
    uint8_t ip_hl:4,        /* header length */
            ip_v:4;         /* version */
#endif
    uint8_t ip_tos;         /* type of service */
    uint16_t ip_len;        /* total length */
    uint16_t ip_id;         /* identification */
    uint16_t ip_off;        /* fragment offset field */
#define IP_DF 0x4000        /* don't fragment flag */
#define IP_MF 0x2000        /* more fragments flag */
#define IP_OFFMASK 0x1fff   /* mask for fragmenting bits */
    uint8_t ip_ttl;         /* time to live */
    uint8_t ip_p;           /* protocol */
    uint16_t ip_sum;        /* checksum */
    struct in_addr ip_src, ip_dst;  /* source and dest address */
} QEMU_PACKED;

static inline bool in6_equal_net(const struct in6_addr *a,
                                 const struct in6_addr *b,
                                 int prefix_len)
{
    if (memcmp(a, b, prefix_len / 8) != 0) {
        return 0;
    }

    if (prefix_len % 8 == 0) {
        return 1;
    }

    return a->s6_addr[prefix_len / 8] >> (8 - (prefix_len % 8))
        == b->s6_addr[prefix_len / 8] >> (8 - (prefix_len % 8));
}

#define TCPS_CLOSED             0       /* closed */
#define TCPS_LISTEN             1       /* listening for connection */
#define TCPS_SYN_SENT           2       /* active, have sent syn */
#define TCPS_SYN_RECEIVED       3       /* have send and received syn */
#define TCPS_ESTABLISHED        4       /* established */
#define TCPS_CLOSE_WAIT         5       /* rcvd fin, waiting for close */
#define TCPS_FIN_WAIT_1         6       /* have closed, sent fin */
#define TCPS_CLOSING            7       /* closed xchd FIN; await FIN ACK */
#define TCPS_LAST_ACK           8       /* had fin and close; await FIN ACK */
#define TCPS_FIN_WAIT_2         9       /* have closed, fin is acked */
#define TCPS_TIME_WAIT          10      /* in 2*msl quiet wait after close */

int net_parse_macaddr(uint8_t *macaddr, const char *p);

#endif /* QEMU_NET_UTIL_H */
