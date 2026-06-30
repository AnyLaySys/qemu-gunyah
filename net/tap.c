#include "qemu/osdep.h"
#include <linux/ioctl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "clients.h"
#include "net/eth.h"
#include "net/net.h"
#include "net/tap.h"
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "standard-headers/linux/virtio_net.h"

#ifndef TUNSETIFF
#define TUNSETIFF _IOW('T', 202, int)
#endif
#ifndef TUNGETFEATURES
#define TUNGETFEATURES _IOR('T', 207, unsigned int)
#endif
#ifndef TUNSETOFFLOAD
#define TUNSETOFFLOAD _IOW('T', 208, unsigned int)
#endif
#ifndef TUNSETVNETHDRSZ
#define TUNSETVNETHDRSZ _IOW('T', 216, int)
#endif
#ifndef TUNSETVNETLE
#define TUNSETVNETLE _IOW('T', 220, int)
#endif
#ifndef TUNSETVNETBE
#define TUNSETVNETBE _IOW('T', 222, int)
#endif
#ifndef IFF_TAP
#define IFF_TAP 0x0002
#endif
#ifndef IFF_NO_PI
#define IFF_NO_PI 0x1000
#endif
#ifndef IFF_VNET_HDR
#define IFF_VNET_HDR 0x4000
#endif
#ifndef TUN_F_USO4
#define TUN_F_USO4 0x20
#endif
#ifndef TUN_F_USO6
#define TUN_F_USO6 0x40
#endif

typedef struct TAPState {
    NetClientState nc;
    int fd;
    uint8_t buf[NET_BUFSIZE];
    bool r, w, vnet, ufo, uso, enabled;
} TAPState;

static void tap_send(void *opaque);
static void tap_writable(void *opaque);

static void tap_update(TAPState *s)
{
    qemu_set_fd_handler(s->fd, s->r && s->enabled ? tap_send : NULL,
                        s->w && s->enabled ? tap_writable : NULL, s);
}

static void tap_read_poll(TAPState *s, bool on)
{
    s->r = on;
    tap_update(s);
}

static void tap_write_poll(TAPState *s, bool on)
{
    s->w = on;
    tap_update(s);
}

static void tap_writable(void *opaque)
{
    TAPState *s = opaque;
    tap_write_poll(s, false);
    qemu_flush_queued_packets(&s->nc);
}

static uint16_t csum16(const void *data, int len)
{
    const uint8_t *p = data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)p[0] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

static void st16(uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v;
}

static void st32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static uint16_t ld16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static size_t tap_iov_to_buf(const struct iovec *iov, int iovcnt, uint8_t *buf,
                             size_t max, size_t *total)
{
    size_t n = 0;
    int i;
    *total = 0;
    for (i = 0; i < iovcnt; i++) {
        size_t len = iov[i].iov_len;
        *total += len;
        if (n < max) {
            size_t c = MIN(len, max - n);
            memcpy(buf + n, iov[i].iov_base, c);
            n += c;
        }
    }
    return n;
}

static bool tap_dhcp(TAPState *s, const struct iovec *iov, int iovcnt,
                     size_t *done)
{
    uint8_t pkt[2048], out[512];
    static const uint8_t mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x57 };
    static const uint8_t srv[4] = { 100, 99, 99, 1 };
    static const uint8_t cli[4] = { 100, 99, 99, 99 };
    static const uint8_t bcast[4] = { 100, 99, 99, 255 };
    static const uint8_t mask[4] = { 255, 255, 255, 0 };
    static const uint8_t dns[8] = { 223, 5, 5, 5, 119, 29, 29, 29 };
    size_t n, off, base, opt, end, o, bp_len, len;
    uint8_t msg = 0, resp;
    uint16_t sport, dport;
    n = tap_iov_to_buf(iov, iovcnt, pkt, sizeof(pkt), done);
    off = s->vnet ? s->nc.vnet_hdr_len : 0;
    if (n < off + 14 + 20 + 8 + 240) {
        return false;
    }
    base = off;
    if (pkt[base + 12] != 0x08 || pkt[base + 13] != 0x00 ||
        (pkt[base + 14] & 0xf0) != 0x40 || pkt[base + 23] != 17) {
        return false;
    }
    opt = base + 14 + ((pkt[base + 14] & 15) << 2);
    if (opt + 8 + 240 > n) {
        return false;
    }
    sport = ld16(pkt + opt);
    dport = ld16(pkt + opt + 2);
    if (sport != 68 || dport != 67) {
        return false;
    }
    base = opt + 8;
    if (pkt[base] != 1 || pkt[base + 1] != 1 || pkt[base + 2] != 6 ||
        memcmp(pkt + base + 236, "\x63\x82\x53\x63", 4)) {
        return false;
    }
    opt = base + 240;
    end = n;
    while (opt < end && pkt[opt] != 255) {
        uint8_t t = pkt[opt++];
        uint8_t l;
        if (!t) {
            continue;
        }
        if (opt >= end) {
            return false;
        }
        l = pkt[opt++];
        if (opt + l > end) {
            return false;
        }
        if (t == 53 && l == 1) {
            msg = pkt[opt];
        }
        opt += l;
    }
    if (msg != 1 && msg != 3) {
        return false;
    }
    resp = msg == 1 ? 2 : 5;
    off = s->vnet ? s->nc.vnet_hdr_len : 0;
    memset(out, 0, sizeof(out));
    memset(out + off, 0xff, 6);
    memcpy(out + off + 6, mac, 6);
    st16(out + off + 12, 0x0800);
    o = off + 14;
    out[o] = 0x45;
    out[o + 8] = 64;
    out[o + 9] = 17;
    memcpy(out + o + 12, srv, 4);
    memset(out + o + 16, 0xff, 4);
    o += 20;
    st16(out + o, 67);
    st16(out + o + 2, 68);
    o += 8;
    out[o] = 2;
    out[o + 1] = 1;
    out[o + 2] = 6;
    memcpy(out + o + 4, pkt + base + 4, 4);
    memcpy(out + o + 10, pkt + base + 10, 2);
    memcpy(out + o + 16, cli, 4);
    memcpy(out + o + 20, srv, 4);
    memcpy(out + o + 28, pkt + base + 28, 16);
    memcpy(out + o + 236, "\x63\x82\x53\x63", 4);
    o += 240;
    out[o++] = 53; out[o++] = 1; out[o++] = resp;
    out[o++] = 54; out[o++] = 4; memcpy(out + o, srv, 4); o += 4;
    out[o++] = 51; out[o++] = 4; st32(out + o, 86400); o += 4;
    out[o++] = 1; out[o++] = 4; memcpy(out + o, mask, 4); o += 4;
    out[o++] = 3; out[o++] = 4; memcpy(out + o, srv, 4); o += 4;
    out[o++] = 6; out[o++] = 8; memcpy(out + o, dns, 8); o += 8;
    out[o++] = 28; out[o++] = 4; memcpy(out + o, bcast, 4); o += 4;
    out[o++] = 255;
    bp_len = o - (off + 14 + 20 + 8);
    len = 20 + 8 + bp_len;
    st16(out + off + 14 + 2, len);
    st16(out + off + 14 + 10, csum16(out + off + 14, 20));
    st16(out + off + 14 + 20 + 4, 8 + bp_len);
    qemu_send_packet_async(&s->nc, out, o, NULL);
    return true;
}

static ssize_t tap_receive_iov(NetClientState *nc, const struct iovec *iov,
                               int iovcnt)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    size_t done;
    ssize_t n;
    if (tap_dhcp(s, iov, iovcnt, &done)) {
        return done;
    }
    n = RETRY_ON_EINTR(writev(s->fd, iov, iovcnt));
    if (n < 0 && errno == EAGAIN) {
        tap_write_poll(s, true);
        return 0;
    }
    return n;
}

static ssize_t tap_receive(NetClientState *nc, const uint8_t *buf, size_t size)
{
    struct iovec iov = { .iov_base = (void *)buf, .iov_len = size };
    return tap_receive_iov(nc, &iov, 1);
}

static void tap_sent(NetClientState *nc, ssize_t len)
{
    tap_read_poll(DO_UPCAST(TAPState, nc, nc), true);
}

static void tap_send(void *opaque)
{
    TAPState *s = opaque;
    int packets = 0;
    for (;;) {
        uint8_t *buf = s->buf;
        uint8_t min_pkt[ETH_ZLEN];
        size_t min_len = sizeof(min_pkt);
        ssize_t n = RETRY_ON_EINTR(read(s->fd, s->buf, sizeof(s->buf)));
        if (n <= 0) {
            return;
        }
        if (net_peer_needs_padding(&s->nc) &&
            eth_pad_short_frame(min_pkt, &min_len, buf, n)) {
            buf = min_pkt;
            n = min_len;
        }
        n = qemu_send_packet_async(&s->nc, buf, n, tap_sent);
        if (n <= 0) {
            if (!n) {
                tap_read_poll(s, false);
            }
            return;
        }
        if (++packets == 50) {
            return;
        }
    }
}

static bool tap_has_ufo(NetClientState *nc)
{
    return DO_UPCAST(TAPState, nc, nc)->ufo;
}

static bool tap_has_uso(NetClientState *nc)
{
    return DO_UPCAST(TAPState, nc, nc)->uso;
}

static bool tap_has_vnet_hdr(NetClientState *nc)
{
    return DO_UPCAST(TAPState, nc, nc)->vnet;
}

static bool tap_has_vnet_hdr_len(NetClientState *nc, int len)
{
    return tap_has_vnet_hdr(nc);
}

static void tap_set_vnet_hdr_len(NetClientState *nc, int len)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    ioctl(s->fd, TUNSETVNETHDRSZ, &len);
}

static int tap_set_vnet_le(NetClientState *nc, bool is_le)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    int v = is_le;
    return ioctl(s->fd, TUNSETVNETLE, &v) ? -errno : 0;
}

static int tap_set_vnet_be(NetClientState *nc, bool is_be)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    int v = is_be;
    return ioctl(s->fd, TUNSETVNETBE, &v) ? -errno : 0;
}

static void tap_set_offload(NetClientState *nc, int csum, int tso4, int tso6,
                            int ecn, int ufo, int uso4, int uso6)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    unsigned flags = 0;
    if (csum) {
        flags |= TUN_F_CSUM;
        if (tso4) {
            flags |= TUN_F_TSO4;
        }
        if (tso6) {
            flags |= TUN_F_TSO6;
        }
        if (ecn) {
            flags |= TUN_F_TSO_ECN;
        }
        if (ufo) {
            flags |= TUN_F_UFO;
        }
        if (uso4) {
            flags |= TUN_F_USO4;
        }
        if (uso6) {
            flags |= TUN_F_USO6;
        }
    }
    ioctl(s->fd, TUNSETOFFLOAD, flags);
}

static void tap_cleanup(NetClientState *nc)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    qemu_purge_queued_packets(nc);
    qemu_set_fd_handler(s->fd, NULL, NULL, NULL);
    close(s->fd);
}

static NetClientInfo tap_info = {
    .type = NET_CLIENT_DRIVER_TAP,
    .size = sizeof(TAPState),
    .receive = tap_receive,
    .receive_iov = tap_receive_iov,
    .cleanup = tap_cleanup,
    .has_ufo = tap_has_ufo,
    .has_uso = tap_has_uso,
    .has_vnet_hdr = tap_has_vnet_hdr,
    .has_vnet_hdr_len = tap_has_vnet_hdr_len,
    .set_offload = tap_set_offload,
    .set_vnet_hdr_len = tap_set_vnet_hdr_len,
    .set_vnet_le = tap_set_vnet_le,
    .set_vnet_be = tap_set_vnet_be,
};

static int tap_open(char *ifname, bool want_vnet, bool need_vnet, bool *vnet,
                    bool *ufo, bool *uso, Error **errp)
{
    static const char *paths[] = { "/dev/net/tun", "/dev/tun" };
    struct ifreq ifr;
    unsigned int features = 0, offload;
    int fd = -1, i, len = sizeof(struct virtio_net_hdr);
    for (i = 0; i < ARRAY_SIZE(paths); i++) {
        fd = RETRY_ON_EINTR(open(paths[i], O_RDWR));
        if (fd >= 0) {
            break;
        }
    }
    if (fd < 0) {
        error_setg_errno(errp, errno, "could not open tun");
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ioctl(fd, TUNGETFEATURES, &features);
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (want_vnet && (features & IFF_VNET_HDR)) {
        ifr.ifr_flags |= IFF_VNET_HDR;
        *vnet = true;
    } else if (need_vnet) {
        error_setg(errp, "vnet_hdr=on requested but unsupported");
        close(fd);
        return -1;
    }
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname[0] ? ifname : "tap%d");
    if (ioctl(fd, TUNSETIFF, &ifr)) {
        error_setg_errno(errp, errno, "could not configure tap");
        close(fd);
        return -1;
    }
    snprintf(ifname, IFNAMSIZ, "%s", ifr.ifr_name);
    if (*vnet) {
        ioctl(fd, TUNSETVNETHDRSZ, &len);
    }
    offload = TUN_F_CSUM | TUN_F_UFO;
    *ufo = ioctl(fd, TUNSETOFFLOAD, offload) == 0;
    offload = TUN_F_CSUM | TUN_F_USO4 | TUN_F_USO6;
    *uso = ioctl(fd, TUNSETOFFLOAD, offload) == 0;
    g_unix_set_fd_nonblocking(fd, true, NULL);
    return fd;
}

int net_init_tap(const Netdev *netdev, const char *name,
                 NetClientState *peer, Error **errp)
{
    const NetdevTapOptions *tap = &netdev->u.tap;
    char ifname[IFNAMSIZ] = "";
    bool vnet = false, ufo = false, uso = false;
    int fd;
    TAPState *s;
    if ((tap->script && strcmp(tap->script, "no")) ||
        (tap->downscript && strcmp(tap->downscript, "no"))) {
        error_setg(errp, "only script=no,downscript=no is supported");
        return -1;
    }
    if (tap->ifname) {
        snprintf(ifname, sizeof(ifname), "%s", tap->ifname);
    }
    fd = tap_open(ifname, !tap->has_vnet_hdr || tap->vnet_hdr,
                  tap->has_vnet_hdr && tap->vnet_hdr, &vnet, &ufo, &uso, errp);
    if (fd < 0) {
        return -1;
    }
    s = DO_UPCAST(TAPState, nc, qemu_new_net_client(&tap_info, peer, "tap", name));
    s->fd = fd;
    s->vnet = vnet;
    s->nc.vnet_hdr_len = vnet ? sizeof(struct virtio_net_hdr) : 0;
    s->ufo = ufo;
    s->uso = uso;
    s->enabled = true;
    qemu_set_info_str(&s->nc, "ifname=%s", ifname);
    tap_read_poll(s, true);
    return 0;
}

int tap_enable(NetClientState *nc)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    s->enabled = true;
    tap_update(s);
    return 0;
}

int tap_disable(NetClientState *nc)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    s->enabled = false;
    tap_update(s);
    return 0;
}

int tap_get_fd(NetClientState *nc)
{
    return DO_UPCAST(TAPState, nc, nc)->fd;
}

struct vhost_net *tap_get_vhost_net(NetClientState *nc)
{
    return NULL;
}
