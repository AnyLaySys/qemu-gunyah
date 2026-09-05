#include "qemu/osdep.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#ifdef __ANDROID__
#include <android/multinetwork.h>
#endif
#include "net/net.h"
#include "qemu/atomic.h"
#include "qemu/error-report.h"
#include "qemu/iov.h"
#include "qemu/main-loop.h"
#include "tap_int.h"

#define SVC_MTU 1500
#define SVC_LEASE 86400
#define SVC_ANSWER 8192

typedef struct SvcPacket {
    const uint8_t *data;
    size_t len;
    uint32_t sip, dip;
    uint16_t sport, dport;
} SvcPacket;

static uint64_t svc_network;

static uint16_t ld16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static void st16(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v;
}

static void st32(uint8_t *p, uint32_t v)
{
    st16(p, v >> 16);
    st16(p + 2, v);
}

static uint32_t sum16(uint32_t sum, const uint8_t *p, size_t len)
{
    while (len > 1) {
        sum += ld16(p);
        p += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint32_t)p[0] << 8;
    }
    return sum;
}

static uint16_t fold16(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

static bool svc_refresh(TAPState *s)
{
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    bool ok;

    if (fd < 0) {
        return false;
    }
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", s->ifname);
    ok = !ioctl(fd, SIOCGIFADDR, &ifr);
    if (ok) {
        s->addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
        ok = !ioctl(fd, SIOCGIFNETMASK, &ifr);
    }
    if (ok) {
        s->mask = ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr;
        ok = !ioctl(fd, SIOCGIFHWADDR, &ifr);
    }
    if (ok) {
        memcpy(s->mac, ifr.ifr_hwaddr.sa_data, 6);
    }
    close(fd);
    if (!ok || !s->mask) {
        s->addr = 0;
    }
    return s->addr != 0;
}

static void svc_send(TAPState *s, const uint8_t *dmac, uint32_t dip,
                     uint16_t sport, uint16_t dport, const uint8_t *data,
                     size_t len)
{
    uint8_t dgram[8 + SVC_ANSWER];
    uint8_t frame[32 + 14 + SVC_MTU];
    uint8_t ph[12];
    size_t off = s->vnet ? s->nc.vnet_hdr_len : 0;
    size_t total = 8 + len, pos = 0;
    uint16_t id = ++s->ip_id, csum;

    if (len > SVC_ANSWER || off > 32) {
        return;
    }
    st16(dgram, sport);
    st16(dgram + 2, dport);
    st16(dgram + 4, total);
    st16(dgram + 6, 0);
    memcpy(dgram + 8, data, len);
    memcpy(ph, &s->addr, 4);
    memcpy(ph + 4, &dip, 4);
    ph[8] = 0;
    ph[9] = 17;
    st16(ph + 10, total);
    csum = fold16(sum16(sum16(0, ph, 12), dgram, total));
    st16(dgram + 6, csum ? csum : 0xffff);
    while (pos < total) {
        size_t chunk = MIN(total - pos, (size_t)(SVC_MTU - 20));
        bool more = pos + chunk < total;
        uint8_t *ip = frame + off + 14;
        size_t n;

        if (more) {
            chunk &= ~(size_t)7;
        }
        memset(frame, 0, off);
        memcpy(frame + off, dmac, 6);
        memcpy(frame + off + 6, s->mac, 6);
        st16(frame + off + 12, 0x0800);
        ip[0] = 0x45;
        ip[1] = 0;
        st16(ip + 2, 20 + chunk);
        st16(ip + 4, id);
        st16(ip + 6, (more ? 0x2000 : 0) | (pos >> 3));
        ip[8] = 64;
        ip[9] = 17;
        st16(ip + 10, 0);
        memcpy(ip + 12, &s->addr, 4);
        memcpy(ip + 16, &dip, 4);
        st16(ip + 10, fold16(sum16(0, ip, 20)));
        memcpy(ip + 20, dgram + pos, chunk);
        n = off + 34 + chunk;
        if (n < off + 60) {
            memset(frame + n, 0, off + 60 - n);
            n = off + 60;
        }
        qemu_send_packet(&s->nc, frame, n);
        pos += chunk;
    }
}

static bool svc_lease(TAPState *s, const uint8_t *mac, uint32_t *ip)
{
    static const uint8_t zero[6];
    uint32_t net = ntohl(s->addr & s->mask);
    uint32_t gw = ntohl(s->addr) - net;
    uint32_t hosts = ntohl(~s->mask);
    uint32_t h;
    int i, idx = -1, spare = -1;

    for (i = 0; i < TAP_LEASES; i++) {
        if (!memcmp(s->leases[i], mac, 6)) {
            idx = i;
            break;
        }
        if (spare < 0 && !memcmp(s->leases[i], zero, 6)) {
            spare = i;
        }
    }
    if (idx < 0) {
        idx = spare >= 0 ? spare : (int)(s->lease_next++ % TAP_LEASES);
        memcpy(s->leases[idx], mac, 6);
    }
    h = idx + 1;
    if (h >= gw) {
        h++;
    }
    if (h >= hosts) {
        return false;
    }
    *ip = htonl(net + h);
    return true;
}

static bool svc_dhcp(TAPState *s, const SvcPacket *pk)
{
    static const uint8_t cookie[4] = { 0x63, 0x82, 0x53, 0x63 };
    static const uint8_t bcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    const uint8_t *b = pk->data, *cid = NULL, *dmac;
    uint8_t r[576], msg = 0, resp, cid_len = 0;
    uint32_t req = 0, sid = 0, ciaddr, yiaddr = 0, dip, bcast;
    size_t i = 240, o;

    if (pk->len < 240 || b[0] != 1 || b[1] != 1 || b[2] != 6 ||
        memcmp(b + 236, cookie, 4)) {
        return false;
    }
    while (i < pk->len && b[i] != 255) {
        uint8_t t = b[i++], l;

        if (!t) {
            continue;
        }
        if (i >= pk->len) {
            break;
        }
        l = b[i++];
        if (i + l > pk->len) {
            break;
        }
        if (t == 53 && l == 1) {
            msg = b[i];
        } else if (t == 50 && l == 4) {
            memcpy(&req, b + i, 4);
        } else if (t == 54 && l == 4) {
            memcpy(&sid, b + i, 4);
        } else if (t == 61 && l) {
            cid = b + i;
            cid_len = l;
        }
        i += l;
    }
    if (!msg || !s->addr) {
        return false;
    }
    memcpy(&ciaddr, b + 12, 4);
    switch (msg) {
    case 1:
        resp = 2;
        break;
    case 3:
    case 8:
        resp = 5;
        break;
    default:
        return true;
    }
    if (msg == 3 && sid && sid != s->addr) {
        return true;
    }
    if (msg != 8) {
        if (!svc_lease(s, b + 28, &yiaddr)) {
            return true;
        }
        if (msg == 3) {
            uint32_t want = req ? req : ciaddr;

            if (want && want != yiaddr) {
                resp = 6;
            }
        }
    }
    memset(r, 0, sizeof(r));
    r[0] = 2;
    r[1] = 1;
    r[2] = 6;
    memcpy(r + 4, b + 4, 4);
    memcpy(r + 10, b + 10, 2);
    if (resp == 5) {
        memcpy(r + 12, b + 12, 4);
    }
    if (resp != 6) {
        memcpy(r + 16, &yiaddr, 4);
        memcpy(r + 20, &s->addr, 4);
    }
    memcpy(r + 24, b + 24, 4);
    memcpy(r + 28, b + 28, 16);
    memcpy(r + 236, cookie, 4);
    o = 240;
    r[o++] = 53;
    r[o++] = 1;
    r[o++] = resp;
    r[o++] = 54;
    r[o++] = 4;
    memcpy(r + o, &s->addr, 4);
    o += 4;
    if (resp != 6) {
        bcast = s->addr | ~s->mask;
        r[o++] = 1;
        r[o++] = 4;
        memcpy(r + o, &s->mask, 4);
        o += 4;
        r[o++] = 3;
        r[o++] = 4;
        memcpy(r + o, &s->addr, 4);
        o += 4;
        r[o++] = 6;
        r[o++] = 4;
        memcpy(r + o, &s->addr, 4);
        o += 4;
        r[o++] = 28;
        r[o++] = 4;
        memcpy(r + o, &bcast, 4);
        o += 4;
        if (msg != 8) {
            r[o++] = 51;
            r[o++] = 4;
            st32(r + o, SVC_LEASE);
            o += 4;
            r[o++] = 58;
            r[o++] = 4;
            st32(r + o, SVC_LEASE / 2);
            o += 4;
            r[o++] = 59;
            r[o++] = 4;
            st32(r + o, SVC_LEASE / 8 * 7);
            o += 4;
        }
    }
    if (cid) {
        r[o++] = 61;
        r[o++] = cid_len;
        memcpy(r + o, cid, cid_len);
        o += cid_len;
    }
    r[o++] = 255;
    if (o < 300) {
        o = 300;
    }
    if (resp == 6) {
        dmac = bcast_mac;
        dip = 0xffffffff;
    } else if (ciaddr) {
        dmac = b + 28;
        dip = ciaddr;
    } else if (b[10] & 0x80) {
        dmac = bcast_mac;
        dip = 0xffffffff;
    } else {
        dmac = b + 28;
        dip = yiaddr;
    }
    svc_send(s, dmac, dip, 67, 68, r, o);
    return true;
}

static size_t dns_question(const uint8_t *m, size_t n)
{
    size_t i = 12;

    if (n < 12 || ld16(m + 4) != 1) {
        return 0;
    }
    while (i < n && m[i]) {
        if (m[i] & 0xc0) {
            i++;
            break;
        }
        i += m[i] + 1;
    }
    i += 5;
    return i <= n ? i : 0;
}

static void svc_dns_fail(TapDns *d)
{
    uint8_t r[512];
    size_t n = dns_question(d->query, d->len);

    if (!n) {
        n = 12;
        memcpy(r, d->query, n);
        st16(r + 4, 0);
    } else {
        memcpy(r, d->query, n);
    }
    r[2] = 0x80 | (r[2] & 0x79);
    r[3] = 0x82;
    st16(r + 6, 0);
    st16(r + 8, 0);
    st16(r + 10, 0);
    svc_send(d->s, d->mac, d->ip, 53, d->port, r, n);
}

#ifdef __ANDROID__
static void svc_dns_ready(void *opaque)
{
    TapDns *d = opaque;
    uint8_t answer[SVC_ANSWER];
    int fd = d->fd, rcode, n;

    qemu_set_fd_handler(fd, NULL, NULL, NULL);
    d->fd = -1;
    n = android_res_nresult(fd, &rcode, answer, sizeof(answer));
    if (n < 12) {
        svc_dns_fail(d);
        return;
    }
    st16(answer, d->id);
    svc_send(d->s, d->mac, d->ip, 53, d->port, answer, n);
}

static void svc_dns_drop(TapDns *d)
{
    if (d->fd >= 0) {
        qemu_set_fd_handler(d->fd, NULL, NULL, NULL);
        android_res_cancel(d->fd);
        d->fd = -1;
    }
}
#endif

static bool svc_dns(TAPState *s, const SvcPacket *pk, const uint8_t *smac)
{
#ifdef __ANDROID__
    TapDns *d = NULL;
    uint64_t network;
    int i, fd;

    if (pk->len < 12) {
        return false;
    }
    for (i = 0; i < TAP_DNS_MAX; i++) {
        if (s->dns[i].fd < 0) {
            d = &s->dns[i];
            break;
        }
    }
    if (!d) {
        d = &s->dns[s->dns_next++ % TAP_DNS_MAX];
        svc_dns_drop(d);
    }
    d->s = s;
    memcpy(d->mac, smac, 6);
    d->ip = pk->sip;
    d->port = pk->sport;
    d->id = ld16(pk->data);
    d->len = MIN(pk->len, sizeof(d->query));
    memcpy(d->query, pk->data, d->len);
    network = qatomic_read(&svc_network);
    if (!network) {
        svc_dns_fail(d);
        return true;
    }
    fd = android_res_nsend(network, pk->data, pk->len, 0);
    if (fd < 0) {
        warn_report_once("dns forward failed: %s", strerror(-fd));
        svc_dns_fail(d);
        return true;
    }
    d->fd = fd;
    qemu_set_fd_handler(fd, svc_dns_ready, NULL, d);
    return true;
#else
    return false;
#endif
}

static bool svc_parse(const uint8_t *p, size_t n, SvcPacket *pk)
{
    const uint8_t *ip = p + 14, *udp = ip + 20;
    size_t iplen, ulen;

    if (n < 42) {
        return false;
    }
    iplen = ld16(ip + 2);
    ulen = ld16(udp + 4);
    if ((ld16(ip + 6) & 0x3fff) || iplen < 28 || iplen > n - 14 ||
        ulen < 8 || ulen > iplen - 20) {
        return false;
    }
    memcpy(&pk->sip, ip + 12, 4);
    memcpy(&pk->dip, ip + 16, 4);
    pk->sport = ld16(udp);
    pk->dport = ld16(udp + 2);
    pk->data = udp + 8;
    pk->len = ulen - 8;
    return true;
}

void tap_svc_init(TAPState *s, const char *ifname)
{
    int i;

    snprintf(s->ifname, sizeof(s->ifname), "%s", ifname);
    for (i = 0; i < TAP_DNS_MAX; i++) {
        s->dns[i].fd = -1;
    }
    svc_refresh(s);
}

void tap_svc_set_network(uint64_t network)
{
    qatomic_set(&svc_network, network);
}

bool tap_svc_input(TAPState *s, const struct iovec *iov, int iovcnt,
                   size_t *len)
{
    uint8_t head[80], pkt[2048];
    size_t off = s->vnet ? s->nc.vnet_hdr_len : 0;
    size_t total = iov_size(iov, iovcnt), n;
    const uint8_t *p = head + off;
    SvcPacket pk;
    uint16_t dport;

    if (off + 42 > sizeof(head) || total < off + 42) {
        return false;
    }
    iov_to_buf(iov, iovcnt, 0, head, off + 42);
    if (ld16(p + 12) != 0x0800 || p[14] != 0x45 || p[23] != 17) {
        return false;
    }
    dport = ld16(p + 36);
    if (dport != 67 && dport != 53) {
        return false;
    }
    if (!s->addr) {
        svc_refresh(s);
    }
    if (dport == 53 && (!s->addr || memcmp(p + 30, &s->addr, 4))) {
        return false;
    }
    n = MIN(total - off, sizeof(pkt));
    iov_to_buf(iov, iovcnt, off, pkt, n);
    if (!svc_parse(pkt, n, &pk)) {
        return false;
    }
    if (dport == 67 ? svc_dhcp(s, &pk) : svc_dns(s, &pk, pkt + 6)) {
        *len = total;
        return true;
    }
    return false;
}

void tap_svc_cleanup(TAPState *s)
{
#ifdef __ANDROID__
    int i;

    for (i = 0; i < TAP_DNS_MAX; i++) {
        svc_dns_drop(&s->dns[i]);
    }
#endif
}
