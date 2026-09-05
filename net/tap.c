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
#include "tap_int.h"

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

static ssize_t tap_receive_iov(NetClientState *nc, const struct iovec *iov,
                               int iovcnt)
{
    TAPState *s = DO_UPCAST(TAPState, nc, nc);
    size_t done;
    ssize_t n;
    if (tap_service_input(s, iov, iovcnt, &done)) {
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
    tap_service_cleanup(s);
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
    tap_service_init(s, ifname);
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
