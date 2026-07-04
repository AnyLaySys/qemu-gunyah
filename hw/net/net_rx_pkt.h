
#ifndef NET_RX_PKT_H
#define NET_RX_PKT_H

#include "net/eth.h"


struct NetRxPkt;

void net_rx_pkt_uninit(struct NetRxPkt *pkt);

void net_rx_pkt_init(struct NetRxPkt **pkt);

size_t net_rx_pkt_get_total_len(struct NetRxPkt *pkt);

void net_rx_pkt_set_protocols(struct NetRxPkt *pkt,
                              const struct iovec *iov, size_t iovcnt,
                              size_t iovoff);

void net_rx_pkt_get_protocols(struct NetRxPkt *pkt,
                                 bool *hasip4, bool *hasip6,
                                 EthL4HdrProto *l4hdr_proto);

size_t net_rx_pkt_get_l4_hdr_offset(struct NetRxPkt *pkt);

size_t net_rx_pkt_get_l5_hdr_offset(struct NetRxPkt *pkt);

eth_ip6_hdr_info *net_rx_pkt_get_ip6_info(struct NetRxPkt *pkt);

eth_ip4_hdr_info *net_rx_pkt_get_ip4_info(struct NetRxPkt *pkt);

typedef enum {
    NetPktRssIpV4,
    NetPktRssIpV4Tcp,
    NetPktRssIpV6Tcp,
    NetPktRssIpV6,
    NetPktRssIpV6Ex,
    NetPktRssIpV6TcpEx,
    NetPktRssIpV4Udp,
    NetPktRssIpV6Udp,
    NetPktRssIpV6UdpEx,
} NetRxPktRssType;

uint32_t
net_rx_pkt_calc_rss_hash(struct NetRxPkt *pkt,
                         NetRxPktRssType type,
                         uint8_t *key);

uint16_t net_rx_pkt_get_ip_id(struct NetRxPkt *pkt);

bool net_rx_pkt_is_tcp_ack(struct NetRxPkt *pkt);

bool net_rx_pkt_has_tcp_data(struct NetRxPkt *pkt);

struct virtio_net_hdr *net_rx_pkt_get_vhdr(struct NetRxPkt *pkt);

eth_pkt_types_e net_rx_pkt_get_packet_type(struct NetRxPkt *pkt);

uint16_t net_rx_pkt_get_vlan_tag(struct NetRxPkt *pkt);

bool net_rx_pkt_is_vlan_stripped(struct NetRxPkt *pkt);

void net_rx_pkt_attach_iovec(struct NetRxPkt *pkt,
                                const struct iovec *iov,
                                int iovcnt, size_t iovoff,
                                bool strip_vlan);

void net_rx_pkt_attach_iovec_ex(struct NetRxPkt *pkt,
                                const struct iovec *iov, int iovcnt,
                                size_t iovoff, int strip_vlan_index,
                                uint16_t vet, uint16_t vet_ext);

static inline void
net_rx_pkt_attach_data(struct NetRxPkt *pkt, const void *data,
                          size_t len, bool strip_vlan)
{
    const struct iovec iov = {
        .iov_base = (void *) data,
        .iov_len = len
    };

    net_rx_pkt_attach_iovec(pkt, &iov, 1, 0, strip_vlan);
}

struct iovec *net_rx_pkt_get_iovec(struct NetRxPkt *pkt);

void net_rx_pkt_dump(struct NetRxPkt *pkt);

void net_rx_pkt_set_vhdr(struct NetRxPkt *pkt,
    struct virtio_net_hdr *vhdr);

void net_rx_pkt_set_vhdr_iovec(struct NetRxPkt *pkt,
    const struct iovec *iov, int iovcnt);

void net_rx_pkt_unset_vhdr(struct NetRxPkt *pkt);

void net_rx_pkt_set_packet_type(struct NetRxPkt *pkt,
    eth_pkt_types_e packet_type);

bool net_rx_pkt_validate_l4_csum(struct NetRxPkt *pkt, bool *csum_valid);

bool net_rx_pkt_validate_l3_csum(struct NetRxPkt *pkt, bool *csum_valid);

bool net_rx_pkt_fix_l4_csum(struct NetRxPkt *pkt);

#endif
