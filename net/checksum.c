
#include "qemu/osdep.h"
#include "net/checksum.h"
#include "net/eth.h"

uint32_t net_checksum_add_cont(int len, uint8_t *buf, int seq)
{
    uint32_t sum1 = 0, sum2 = 0;
    int i;

    for (i = 0; i < len - 1; i += 2) {
        sum1 += (uint32_t)buf[i];
        sum2 += (uint32_t)buf[i + 1];
    }
    if (i < len) {
        sum1 += (uint32_t)buf[i];
    }

    if (seq & 1) {
        return sum1 + (sum2 << 8);
    } else {
        return sum2 + (sum1 << 8);
    }
}

uint16_t net_checksum_finish(uint32_t sum)
{
    while (sum>>16)
        sum = (sum & 0xFFFF)+(sum >> 16);
    return ~sum;
}

uint16_t net_checksum_tcpudp(uint16_t length, uint16_t proto,
                             uint8_t *addrs, uint8_t *buf)
{
    uint32_t sum = 0;

    sum += net_checksum_add(length, buf);
    sum += net_checksum_add(8, addrs);
    sum += proto + length;
    return net_checksum_finish(sum);
}

void net_checksum_calculate(void *data, int length, int csum_flag)
{
    int mac_hdr_len, ip_len;
    struct ip_header *ip;
    uint16_t csum;


    if (length < sizeof(struct eth_header)) {
        return;
    }

    switch (lduw_be_p(&PKT_GET_ETH_HDR(data)->h_proto)) {
    case ETH_P_VLAN:
        mac_hdr_len = sizeof(struct eth_header) +
                     sizeof(struct vlan_header);
        break;
    case ETH_P_DVLAN:
        if (lduw_be_p(&PKT_GET_VLAN_HDR(data)->h_proto) == ETH_P_VLAN) {
            mac_hdr_len = sizeof(struct eth_header) +
                         2 * sizeof(struct vlan_header);
        } else {
            mac_hdr_len = sizeof(struct eth_header) +
                         sizeof(struct vlan_header);
        }
        break;
    default:
        mac_hdr_len = sizeof(struct eth_header);
        break;
    }

    length -= mac_hdr_len;

    if (length < sizeof(struct ip_header)) {
        return;
    }

    ip = (struct ip_header *)((uint8_t *)data + mac_hdr_len);

    if (IP_HEADER_VERSION(ip) != IP_HEADER_VERSION_4) {
        return; /* not IPv4 */
    }

    if (csum_flag & CSUM_IP) {
        stw_he_p(&ip->ip_sum, 0);
        csum = net_raw_checksum((uint8_t *)ip, IP_HDR_GET_LEN(ip));
        stw_be_p(&ip->ip_sum, csum);
    }

    if (IP4_IS_FRAGMENT(ip)) {
        return; /* a fragmented IP packet */
    }

    ip_len = lduw_be_p(&ip->ip_len);

    if (length < ip_len) {
        return;
    }

    ip_len -= IP_HDR_GET_LEN(ip);

    switch (ip->ip_p) {
    case IP_PROTO_TCP:
    {
        if (!(csum_flag & CSUM_TCP)) {
            return;
        }

        tcp_header *tcp = (tcp_header *)(ip + 1);

        if (ip_len < sizeof(tcp_header)) {
            return;
        }

        stw_he_p(&tcp->th_sum, 0);

        csum = net_checksum_tcpudp(ip_len, ip->ip_p,
                                   (uint8_t *)&ip->ip_src,
                                   (uint8_t *)tcp);

        stw_be_p(&tcp->th_sum, csum);

        break;
    }
    case IP_PROTO_UDP:
    {
        if (!(csum_flag & CSUM_UDP)) {
            return;
        }

        udp_header *udp = (udp_header *)(ip + 1);

        if (ip_len < sizeof(udp_header)) {
            return;
        }

        stw_he_p(&udp->uh_sum, 0);

        csum = net_checksum_tcpudp(ip_len, ip->ip_p,
                                   (uint8_t *)&ip->ip_src,
                                   (uint8_t *)udp);

        stw_be_p(&udp->uh_sum, csum);

        break;
    }
    default:
        break;
    }
}

uint32_t
net_checksum_add_iov(const struct iovec *iov, const unsigned int iov_cnt,
                     uint32_t iov_off, uint32_t size, uint32_t csum_offset)
{
    size_t iovec_off;
    unsigned int i;
    uint32_t res = 0;

    iovec_off = 0;
    for (i = 0; i < iov_cnt && size; i++) {
        if (iov_off < (iovec_off + iov[i].iov_len)) {
            size_t len = MIN((iovec_off + iov[i].iov_len) - iov_off , size);
            void *chunk_buf = iov[i].iov_base + (iov_off - iovec_off);

            res += net_checksum_add_cont(len, chunk_buf, csum_offset);
            csum_offset += len;

            iov_off += len;
            size -= len;
        }
        iovec_off += iov[i].iov_len;
    }
    return res;
}
