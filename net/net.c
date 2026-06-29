
#include "qemu/osdep.h"

#include "net/net.h"
#include "clients.h"
#include "hw/qdev-properties.h"
#include "net/eth.h"
#include "util.h"

#include "monitor/monitor.h"
#include "qemu/help_option.h"
#include "qapi/qapi-visit-net.h"
#include "qobject/qdict.h"
#include "qapi/qmp/qerror.h"
#include "qemu/error-report.h"
#include "qemu/sockets.h"
#include "qemu/cutils.h"
#include "qemu/config-file.h"
#include "qemu/ctype.h"
#include "qemu/id.h"
#include "qemu/iov.h"
#include "qemu/qemu-print.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"
#include "qemu/keyval.h"
#include "qapi/error.h"
#include "qapi/opts-visitor.h"
#include "system/runstate.h"
#include "qapi/string-output-visitor.h"
#include "qapi/qobject-input-visitor.h"
#include "standard-headers/linux/virtio_net.h"

static VMChangeStateEntry *net_change_state_entry;
NetClientStateList net_clients;

typedef struct NetdevQueueEntry {
    Netdev *nd;
    Location loc;
    QSIMPLEQ_ENTRY(NetdevQueueEntry) entry;
} NetdevQueueEntry;

typedef QSIMPLEQ_HEAD(, NetdevQueueEntry) NetdevQueue;

static NetdevQueue nd_queue = QSIMPLEQ_HEAD_INITIALIZER(nd_queue);

static GHashTable *nic_model_help;

static int nb_nics;
static NICInfo nd_table[MAX_NICS];


int convert_host_port(struct sockaddr_in *saddr, const char *host,
                      const char *port, Error **errp)
{
    struct hostent *he;
    const char *r;
    long p;

    memset(saddr, 0, sizeof(*saddr));

    saddr->sin_family = AF_INET;
    if (host[0] == '\0') {
        saddr->sin_addr.s_addr = 0;
    } else {
        if (qemu_isdigit(host[0])) {
            if (!inet_aton(host, &saddr->sin_addr)) {
                error_setg(errp, "host address '%s' is not a valid "
                           "IPv4 address", host);
                return -1;
            }
        } else {
            he = gethostbyname(host);
            if (he == NULL) {
                error_setg(errp, "can't resolve host address '%s'", host);
                return -1;
            }
            saddr->sin_addr = *(struct in_addr *)he->h_addr;
        }
    }
    if (qemu_strtol(port, &r, 0, &p) != 0) {
        error_setg(errp, "port number '%s' is invalid", port);
        return -1;
    }
    saddr->sin_port = htons(p);
    return 0;
}

int parse_host_port(struct sockaddr_in *saddr, const char *str,
                    Error **errp)
{
    gchar **substrings;
    int ret;

    substrings = g_strsplit(str, ":", 2);
    if (!substrings || !substrings[0] || !substrings[1]) {
        error_setg(errp, "host address '%s' doesn't contain ':' "
                   "separating host from port", str);
        ret = -1;
        goto out;
    }

    ret = convert_host_port(saddr, substrings[0], substrings[1], errp);

out:
    g_strfreev(substrings);
    return ret;
}

char *qemu_mac_strdup_printf(const uint8_t *macaddr)
{
    return g_strdup_printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                           macaddr[0], macaddr[1], macaddr[2],
                           macaddr[3], macaddr[4], macaddr[5]);
}

void qemu_set_info_str(NetClientState *nc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(nc->info_str, sizeof(nc->info_str), fmt, ap);
    va_end(ap);
}

void qemu_format_nic_info_str(NetClientState *nc, uint8_t macaddr[6])
{
    qemu_set_info_str(nc, "model=%s,macaddr=%02x:%02x:%02x:%02x:%02x:%02x",
                      nc->model, macaddr[0], macaddr[1], macaddr[2],
                      macaddr[3], macaddr[4], macaddr[5]);
}

static int mac_table[256] = {0};

static void qemu_macaddr_set_used(MACAddr *macaddr)
{
    int index;

    for (index = 0x56; index < 0xFF; index++) {
        if (macaddr->a[5] == index) {
            mac_table[index]++;
        }
    }
}

static void qemu_macaddr_set_free(MACAddr *macaddr)
{
    int index;
    static const MACAddr base = { .a = { 0x52, 0x54, 0x00, 0x12, 0x34, 0 } };

    if (memcmp(macaddr->a, &base.a, (sizeof(base.a) - 1)) != 0) {
        return;
    }
    for (index = 0x56; index < 0xFF; index++) {
        if (macaddr->a[5] == index) {
            mac_table[index]--;
        }
    }
}

static int qemu_macaddr_get_free(void)
{
    int index;

    for (index = 0x56; index < 0xFF; index++) {
        if (mac_table[index] == 0) {
            return index;
        }
    }

    return -1;
}

void qemu_macaddr_default_if_unset(MACAddr *macaddr)
{
    static const MACAddr zero = { .a = { 0,0,0,0,0,0 } };
    static const MACAddr base = { .a = { 0x52, 0x54, 0x00, 0x12, 0x34, 0 } };

    if (memcmp(macaddr, &zero, sizeof(zero)) != 0) {
        if (memcmp(macaddr->a, &base.a, (sizeof(base.a) - 1)) != 0) {
            return;
        } else {
            qemu_macaddr_set_used(macaddr);
            return;
        }
    }

    macaddr->a[0] = 0x52;
    macaddr->a[1] = 0x54;
    macaddr->a[2] = 0x00;
    macaddr->a[3] = 0x12;
    macaddr->a[4] = 0x34;
    macaddr->a[5] = qemu_macaddr_get_free();
    qemu_macaddr_set_used(macaddr);
}

static char *assign_name(NetClientState *nc1, const char *model)
{
    NetClientState *nc;
    int id = 0;

    QTAILQ_FOREACH(nc, &net_clients, next) {
        if (nc == nc1) {
            continue;
        }
        if (strcmp(nc->model, model) == 0) {
            id++;
        }
    }

    return g_strdup_printf("%s.%d", model, id);
}

static void qemu_net_client_destructor(NetClientState *nc)
{
    g_free(nc);
}
static ssize_t qemu_deliver_packet_iov(NetClientState *sender,
                                       unsigned flags,
                                       const struct iovec *iov,
                                       int iovcnt,
                                       void *opaque);

static void qemu_net_client_setup(NetClientState *nc,
                                  NetClientInfo *info,
                                  NetClientState *peer,
                                  const char *model,
                                  const char *name,
                                  NetClientDestructor *destructor,
                                  bool is_datapath)
{
    nc->info = info;
    nc->model = g_strdup(model);
    if (name) {
        nc->name = g_strdup(name);
    } else {
        nc->name = assign_name(nc, model);
    }

    if (peer) {
        assert(!peer->peer);
        nc->peer = peer;
        peer->peer = nc;
    }
    QTAILQ_INSERT_TAIL(&net_clients, nc, next);

    nc->incoming_queue = qemu_new_net_queue(qemu_deliver_packet_iov, nc);
    nc->destructor = destructor;
    nc->is_datapath = is_datapath;
}

NetClientState *qemu_new_net_client(NetClientInfo *info,
                                    NetClientState *peer,
                                    const char *model,
                                    const char *name)
{
    NetClientState *nc;

    assert(info->size >= sizeof(NetClientState));

    nc = g_malloc0(info->size);
    qemu_net_client_setup(nc, info, peer, model, name,
                          qemu_net_client_destructor, true);

    return nc;
}

NetClientState *qemu_new_net_control_client(NetClientInfo *info,
                                            NetClientState *peer,
                                            const char *model,
                                            const char *name)
{
    NetClientState *nc;

    assert(info->size >= sizeof(NetClientState));

    nc = g_malloc0(info->size);
    qemu_net_client_setup(nc, info, peer, model, name,
                          qemu_net_client_destructor, false);

    return nc;
}

NICState *qemu_new_nic(NetClientInfo *info,
                       NICConf *conf,
                       const char *model,
                       const char *name,
                       MemReentrancyGuard *reentrancy_guard,
                       void *opaque)
{
    NetClientState **peers = conf->peers.ncs;
    NICState *nic;
    int i, queues = MAX(1, conf->peers.queues);

    assert(info->type == NET_CLIENT_DRIVER_NIC);
    assert(info->size >= sizeof(NICState));

    nic = g_malloc0(info->size + sizeof(NetClientState) * queues);
    nic->ncs = (void *)nic + info->size;
    nic->conf = conf;
    nic->reentrancy_guard = reentrancy_guard,
    nic->opaque = opaque;

    for (i = 0; i < queues; i++) {
        qemu_net_client_setup(&nic->ncs[i], info, peers[i], model, name,
                              NULL, true);
        nic->ncs[i].queue_index = i;
    }

    return nic;
}

NetClientState *qemu_get_subqueue(NICState *nic, int queue_index)
{
    return nic->ncs + queue_index;
}

NetClientState *qemu_get_queue(NICState *nic)
{
    return qemu_get_subqueue(nic, 0);
}

NICState *qemu_get_nic(NetClientState *nc)
{
    NetClientState *nc0 = nc - nc->queue_index;

    return (NICState *)((void *)nc0 - nc->info->size);
}

void *qemu_get_nic_opaque(NetClientState *nc)
{
    NICState *nic = qemu_get_nic(nc);

    return nic->opaque;
}

NetClientState *qemu_get_peer(NetClientState *nc, int queue_index)
{
    assert(nc != NULL);
    NetClientState *ncs = nc + queue_index;
    return ncs->peer;
}

static void qemu_cleanup_net_client(NetClientState *nc,
                                    bool remove_from_net_clients)
{
    if (remove_from_net_clients) {
        QTAILQ_REMOVE(&net_clients, nc, next);
    }

    if (nc->info->cleanup) {
        nc->info->cleanup(nc);
    }
}

static void qemu_free_net_client(NetClientState *nc)
{
    if (nc->incoming_queue) {
        qemu_del_net_queue(nc->incoming_queue);
    }
    if (nc->peer) {
        nc->peer->peer = NULL;
    }
    g_free(nc->name);
    g_free(nc->model);
    if (nc->destructor) {
        nc->destructor(nc);
    }
}

void qemu_del_net_client(NetClientState *nc)
{
    NetClientState *ncs[MAX_QUEUE_NUM];
    int queues, i;

    assert(nc->info->type != NET_CLIENT_DRIVER_NIC);

    queues = qemu_find_net_clients_except(nc->name, ncs,
                                          NET_CLIENT_DRIVER_NIC,
                                          MAX_QUEUE_NUM);
    assert(queues != 0);

    if (nc->peer && nc->peer->info->type == NET_CLIENT_DRIVER_NIC) {
        NICState *nic = qemu_get_nic(nc->peer);
        if (nic->peer_deleted) {
            return;
        }
        nic->peer_deleted = true;

        for (i = 0; i < queues; i++) {
            ncs[i]->peer->link_down = true;
            QTAILQ_REMOVE(&net_clients, ncs[i], next);
        }

        if (nc->peer->info->link_status_changed) {
            nc->peer->info->link_status_changed(nc->peer);
        }

        return;
    }

    for (i = 0; i < queues; i++) {
        qemu_cleanup_net_client(ncs[i], true);
        qemu_free_net_client(ncs[i]);
    }
}

void qemu_del_nic(NICState *nic)
{
    int i, queues = MAX(nic->conf->peers.queues, 1);

    qemu_macaddr_set_free(&nic->conf->macaddr);

    for (i = 0; i < queues; i++) {
        NetClientState *nc = qemu_get_subqueue(nic, i);
        if (nic->peer_deleted) {
            qemu_cleanup_net_client(nc->peer, false);
            qemu_free_net_client(nc->peer);
        } else if (nc->peer) {
            qemu_purge_queued_packets(nc->peer);
        }
    }

    for (i = queues - 1; i >= 0; i--) {
        NetClientState *nc = qemu_get_subqueue(nic, i);

        qemu_cleanup_net_client(nc, true);
        qemu_free_net_client(nc);
    }

    g_free(nic);
}

void qemu_foreach_nic(qemu_nic_foreach func, void *opaque)
{
    NetClientState *nc;

    QTAILQ_FOREACH(nc, &net_clients, next) {
        if (nc->info->type == NET_CLIENT_DRIVER_NIC) {
            if (nc->queue_index == 0) {
                func(qemu_get_nic(nc), opaque);
            }
        }
    }
}

bool qemu_has_ufo(NetClientState *nc)
{
    if (!nc || !nc->info->has_ufo) {
        return false;
    }

    return nc->info->has_ufo(nc);
}

bool qemu_has_uso(NetClientState *nc)
{
    if (!nc || !nc->info->has_uso) {
        return false;
    }

    return nc->info->has_uso(nc);
}

bool qemu_has_vnet_hdr(NetClientState *nc)
{
    if (!nc || !nc->info->has_vnet_hdr) {
        return false;
    }

    return nc->info->has_vnet_hdr(nc);
}

bool qemu_has_vnet_hdr_len(NetClientState *nc, int len)
{
    if (!nc || !nc->info->has_vnet_hdr_len) {
        return false;
    }

    return nc->info->has_vnet_hdr_len(nc, len);
}

void qemu_set_offload(NetClientState *nc, int csum, int tso4, int tso6,
                          int ecn, int ufo, int uso4, int uso6)
{
    if (!nc || !nc->info->set_offload) {
        return;
    }

    nc->info->set_offload(nc, csum, tso4, tso6, ecn, ufo, uso4, uso6);
}

int qemu_get_vnet_hdr_len(NetClientState *nc)
{
    if (!nc) {
        return 0;
    }

    return nc->vnet_hdr_len;
}

void qemu_set_vnet_hdr_len(NetClientState *nc, int len)
{
    if (!nc || !nc->info->set_vnet_hdr_len) {
        return;
    }

    assert(len == sizeof(struct virtio_net_hdr_mrg_rxbuf) ||
           len == sizeof(struct virtio_net_hdr) ||
           len == sizeof(struct virtio_net_hdr_v1_hash));

    nc->vnet_hdr_len = len;
    nc->info->set_vnet_hdr_len(nc, len);
}

int qemu_set_vnet_le(NetClientState *nc, bool is_le)
{
#if HOST_BIG_ENDIAN
    if (!nc || !nc->info->set_vnet_le) {
        return -ENOSYS;
    }

    return nc->info->set_vnet_le(nc, is_le);
#else
    return 0;
#endif
}

int qemu_set_vnet_be(NetClientState *nc, bool is_be)
{
#if HOST_BIG_ENDIAN
    return 0;
#else
    if (!nc || !nc->info->set_vnet_be) {
        return -ENOSYS;
    }

    return nc->info->set_vnet_be(nc, is_be);
#endif
}

int qemu_can_receive_packet(NetClientState *nc)
{
    if (nc->receive_disabled) {
        return 0;
    } else if (nc->info->can_receive &&
               !nc->info->can_receive(nc)) {
        return 0;
    }
    return 1;
}

int qemu_can_send_packet(NetClientState *sender)
{
    int vm_running = runstate_is_running();

    if (!vm_running) {
        return 0;
    }

    if (!sender->peer) {
        return 1;
    }

    return qemu_can_receive_packet(sender->peer);
}

void qemu_purge_queued_packets(NetClientState *nc)
{
    if (!nc->peer) {
        return;
    }

    qemu_net_queue_purge(nc->peer->incoming_queue, nc);
}

void qemu_flush_or_purge_queued_packets(NetClientState *nc, bool purge)
{
    nc->receive_disabled = 0;

    if (qemu_net_queue_flush(nc->incoming_queue)) {
        qemu_notify_event();
    } else if (purge) {
        qemu_net_queue_purge(nc->incoming_queue, nc->peer);
    }
}

void qemu_flush_queued_packets(NetClientState *nc)
{
    qemu_flush_or_purge_queued_packets(nc, false);
}

static ssize_t qemu_send_packet_async_with_flags(NetClientState *sender,
                                                 unsigned flags,
                                                 const uint8_t *buf, int size,
                                                 NetPacketSent *sent_cb)
{
    NetQueue *queue;

#ifdef DEBUG_NET
    printf("qemu_send_packet_async:\n");
    qemu_hexdump(stdout, "net", buf, size);
#endif

    if (sender->link_down || !sender->peer) {
        return size;
    }

    queue = sender->peer->incoming_queue;

    return qemu_net_queue_send(queue, sender, flags, buf, size, sent_cb);
}

ssize_t qemu_send_packet_async(NetClientState *sender,
                               const uint8_t *buf, int size,
                               NetPacketSent *sent_cb)
{
    return qemu_send_packet_async_with_flags(sender, QEMU_NET_PACKET_FLAG_NONE,
                                             buf, size, sent_cb);
}

ssize_t qemu_send_packet(NetClientState *nc, const uint8_t *buf, int size)
{
    return qemu_send_packet_async(nc, buf, size, NULL);
}

ssize_t qemu_receive_packet(NetClientState *nc, const uint8_t *buf, int size)
{
    if (!qemu_can_receive_packet(nc)) {
        return 0;
    }

    return qemu_net_queue_receive(nc->incoming_queue, buf, size);
}

ssize_t qemu_send_packet_raw(NetClientState *nc, const uint8_t *buf, int size)
{
    return qemu_send_packet_async_with_flags(nc, QEMU_NET_PACKET_FLAG_RAW,
                                             buf, size, NULL);
}

static ssize_t nc_sendv_compat(NetClientState *nc, const struct iovec *iov,
                               int iovcnt, unsigned flags)
{
    uint8_t *buf = NULL;
    uint8_t *buffer;
    size_t offset;
    ssize_t ret;

    if (iovcnt == 1) {
        buffer = iov[0].iov_base;
        offset = iov[0].iov_len;
    } else {
        offset = iov_size(iov, iovcnt);
        if (offset > NET_BUFSIZE) {
            return -1;
        }
        buf = g_malloc(offset);
        buffer = buf;
        offset = iov_to_buf(iov, iovcnt, 0, buf, offset);
    }

    ret = nc->info->receive(nc, buffer, offset);

    g_free(buf);
    return ret;
}

static ssize_t qemu_deliver_packet_iov(NetClientState *sender,
                                       unsigned flags,
                                       const struct iovec *iov,
                                       int iovcnt,
                                       void *opaque)
{
    MemReentrancyGuard *owned_reentrancy_guard;
    NetClientState *nc = opaque;
    int ret;
    struct virtio_net_hdr_v1_hash vnet_hdr = { };
    g_autofree struct iovec *iov_copy = NULL;


    if (nc->link_down) {
        return iov_size(iov, iovcnt);
    }

    if (nc->receive_disabled) {
        return 0;
    }

    if (nc->info->type != NET_CLIENT_DRIVER_NIC ||
        qemu_get_nic(nc)->reentrancy_guard->engaged_in_io) {
        owned_reentrancy_guard = NULL;
    } else {
        owned_reentrancy_guard = qemu_get_nic(nc)->reentrancy_guard;
        owned_reentrancy_guard->engaged_in_io = true;
    }

    if ((flags & QEMU_NET_PACKET_FLAG_RAW) && nc->vnet_hdr_len) {
        iov_copy = g_new(struct iovec, iovcnt + 1);
        iov_copy[0].iov_base = &vnet_hdr;
        iov_copy[0].iov_len =  nc->vnet_hdr_len;
        memcpy(&iov_copy[1], iov, iovcnt * sizeof(*iov));
        iov = iov_copy;
        iovcnt++;
    }

    if (nc->info->receive_iov) {
        ret = nc->info->receive_iov(nc, iov, iovcnt);
    } else {
        ret = nc_sendv_compat(nc, iov, iovcnt, flags);
    }

    if (owned_reentrancy_guard) {
        owned_reentrancy_guard->engaged_in_io = false;
    }

    if (ret == 0) {
        nc->receive_disabled = 1;
    }

    return ret;
}

ssize_t qemu_sendv_packet_async(NetClientState *sender,
                                const struct iovec *iov, int iovcnt,
                                NetPacketSent *sent_cb)
{
    NetQueue *queue;
    size_t size = iov_size(iov, iovcnt);

    if (size > NET_BUFSIZE) {
        return size;
    }

    if (sender->link_down || !sender->peer) {
        return size;
    }

    queue = sender->peer->incoming_queue;

    return qemu_net_queue_send_iov(queue, sender,
                                   QEMU_NET_PACKET_FLAG_NONE,
                                   iov, iovcnt, sent_cb);
}

ssize_t
qemu_sendv_packet(NetClientState *nc, const struct iovec *iov, int iovcnt)
{
    return qemu_sendv_packet_async(nc, iov, iovcnt, NULL);
}

NetClientState *qemu_find_netdev(const char *id)
{
    NetClientState *nc;

    QTAILQ_FOREACH(nc, &net_clients, next) {
        if (nc->info->type == NET_CLIENT_DRIVER_NIC)
            continue;
        if (!strcmp(nc->name, id)) {
            return nc;
        }
    }

    return NULL;
}

int qemu_find_net_clients_except(const char *id, NetClientState **ncs,
                                 NetClientDriver type, int max)
{
    NetClientState *nc;
    int ret = 0;

    QTAILQ_FOREACH(nc, &net_clients, next) {
        if (nc->info->type == type) {
            continue;
        }
        if (!id || !strcmp(nc->name, id)) {
            if (ret < max) {
                ncs[ret] = nc;
            }
            ret++;
        }
    }

    return ret;
}

static int nic_get_free_idx(void)
{
    int index;

    for (index = 0; index < MAX_NICS; index++)
        if (!nd_table[index].used)
            return index;
    return -1;
}

GPtrArray *qemu_get_nic_models(const char *device_type)
{
    GPtrArray *nic_models = g_ptr_array_new();
    GSList *list = object_class_get_list_sorted(device_type, false);

    while (list) {
        DeviceClass *dc = OBJECT_CLASS_CHECK(DeviceClass, list->data,
                                             TYPE_DEVICE);
        GSList *next;
        if (test_bit(DEVICE_CATEGORY_NETWORK, dc->categories) &&
            dc->user_creatable) {
            const char *name = object_class_get_name(list->data);
            Object *obj = object_new_with_class(OBJECT_CLASS(dc));
            if (object_property_find(obj, "netdev")) {
                g_ptr_array_add(nic_models, (gpointer)name);
            }
            object_unref(obj);
        }
        next = list->next;
        g_slist_free_1(list);
        list = next;
    }
    g_ptr_array_add(nic_models, NULL);

    return nic_models;
}

static int net_init_nic(const Netdev *netdev, const char *name,
                        NetClientState *peer, Error **errp)
{
    int idx;
    NICInfo *nd;
    const NetLegacyNicOptions *nic;

    assert(netdev->type == NET_CLIENT_DRIVER_NIC);
    nic = &netdev->u.nic;

    idx = nic_get_free_idx();
    if (idx == -1 || nb_nics >= MAX_NICS) {
        error_setg(errp, "too many NICs");
        return -1;
    }

    nd = &nd_table[idx];

    memset(nd, 0, sizeof(*nd));

    if (nic->netdev) {
        nd->netdev = qemu_find_netdev(nic->netdev);
        if (!nd->netdev) {
            error_setg(errp, "netdev '%s' not found", nic->netdev);
            return -1;
        }
    } else {
        if (!peer) {
            return 0;
        }
        nd->netdev = peer;
    }
    nd->name = g_strdup(name);
    if (nic->model) {
        nd->model = g_strdup(nic->model);
    }
    if (nic->addr) {
        nd->devaddr = g_strdup(nic->addr);
    }

    if (nic->macaddr &&
        net_parse_macaddr(nd->macaddr.a, nic->macaddr) < 0) {
        error_setg(errp, "invalid syntax for ethernet address");
        return -1;
    }
    if (nic->macaddr &&
        is_multicast_ether_addr(nd->macaddr.a)) {
        error_setg(errp,
                   "NIC cannot have multicast MAC address (odd 1st byte)");
        return -1;
    }
    qemu_macaddr_default_if_unset(&nd->macaddr);

    if (nic->has_vectors) {
        if (nic->vectors > 0x7ffffff) {
            error_setg(errp, "invalid # of vectors: %"PRIu32, nic->vectors);
            return -1;
        }
        nd->nvectors = nic->vectors;
    } else {
        nd->nvectors = DEV_NVECTORS_UNSPECIFIED;
    }

    nd->used = 1;
    nb_nics++;

    return idx;
}

static gboolean add_nic_result(gpointer key, gpointer value, gpointer user_data)
{
    GPtrArray *results = user_data;
    GPtrArray *alias_list = value;
    const char *model = key;
    char *result;

    if (!alias_list) {
        result = g_strdup(model);
    } else {
        GString *result_str = g_string_new(model);
        int i;

        g_string_append(result_str, " (aka ");
        for (i = 0; i < alias_list->len; i++) {
            if (i) {
                g_string_append(result_str, ", ");
            }
            g_string_append(result_str, alias_list->pdata[i]);
        }
        g_string_append(result_str, ")");
        result = result_str->str;
        g_string_free(result_str, false);
        g_ptr_array_unref(alias_list);
    }
    g_ptr_array_add(results, result);
    return true;
}

static int model_cmp(char **a, char **b)
{
    return strcmp(*a, *b);
}

static void show_nic_models(void)
{
    GPtrArray *results = g_ptr_array_new();
    int i;

    g_hash_table_foreach_remove(nic_model_help, add_nic_result, results);
    g_ptr_array_sort(results, (GCompareFunc)model_cmp);

    printf("Available NIC models for this configuration:\n");
    for (i = 0 ; i < results->len; i++) {
        printf("%s\n", (char *)results->pdata[i]);
    }
    g_hash_table_unref(nic_model_help);
    nic_model_help = NULL;
}

static void add_nic_model_help(const char *model, const char *alias)
{
    GPtrArray *alias_list = NULL;

    if (g_hash_table_lookup_extended(nic_model_help, model, NULL,
                                     (gpointer *)&alias_list)) {
        if (!alias) {
            return;
        }
        if (alias_list) {
            if (!g_ptr_array_find_with_equal_func(alias_list, alias,
                                                  g_str_equal, NULL)) {
                g_ptr_array_add(alias_list, g_strdup(alias));
            }
            return;
        }
    }
    if (alias) {
        alias_list = g_ptr_array_new();
        g_ptr_array_set_free_func(alias_list, g_free);
        g_ptr_array_add(alias_list, g_strdup(alias));
    }
    g_hash_table_replace(nic_model_help, g_strdup(model), alias_list);
}

NICInfo *qemu_find_nic_info(const char *typename, bool match_default,
                            const char *alias)
{
    NICInfo *nd;
    int i;

    if (nic_model_help) {
        add_nic_model_help(typename, alias);
    }

    for (i = 0; i < nb_nics; i++) {
        nd = &nd_table[i];

        if (!nd->used || nd->instantiated) {
            continue;
        }

        if ((match_default && !nd->model) || !g_strcmp0(nd->model, typename)
            || (alias && !g_strcmp0(nd->model, alias))) {
            return nd;
        }
    }
    return NULL;
}

static bool is_nic_model_help_option(const char *model)
{
    if (model && is_help_option(model)) {
        if (!nic_model_help) {
            nic_model_help = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                   g_free, NULL);
        }
        return true;
    }
    return false;
}

bool qemu_configure_nic_device(DeviceState *dev, bool match_default,
                               const char *alias)
{
    NICInfo *nd = qemu_find_nic_info(object_get_typename(OBJECT(dev)),
                                     match_default, alias);

    if (nd) {
        qdev_set_nic_properties(dev, nd);
        return true;
    }
    return false;
}

DeviceState *qemu_create_nic_device(const char *typename, bool match_default,
                                    const char *alias)
{
    NICInfo *nd = qemu_find_nic_info(typename, match_default, alias);
    DeviceState *dev;

    if (!nd) {
        return NULL;
    }

    dev = qdev_new(typename);
    qdev_set_nic_properties(dev, nd);
    return dev;
}

void qemu_create_nic_bus_devices(BusState *bus, const char *parent_type,
                                 const char *default_model,
                                 const char *alias, const char *alias_target)
{
    GPtrArray *nic_models = qemu_get_nic_models(parent_type);
    const char *model;
    DeviceState *dev;
    NICInfo *nd;
    int i;

    if (nic_model_help) {
        if (alias_target) {
            add_nic_model_help(alias_target, alias);
        }
        for (i = 0; i < nic_models->len - 1; i++) {
            add_nic_model_help(nic_models->pdata[i], NULL);
        }
    }

    nic_models->len--;

    for (i = 0; i < nb_nics; i++) {
        nd = &nd_table[i];

        if (!nd->used || nd->instantiated) {
            continue;
        }

        model = nd->model ? nd->model : default_model;
        if (!model) {
            continue;
        }

        if (g_str_equal(model, alias)) {
            model = alias_target;
        }

        if (!g_ptr_array_find_with_equal_func(nic_models, model,
                                              g_str_equal, NULL)) {
            continue;
        }

        dev = qdev_new(model);
        qdev_set_nic_properties(dev, nd);
        qdev_realize_and_unref(dev, bus, &error_fatal);
    }

    g_ptr_array_free(nic_models, true);
}

static int (* const net_client_init_fun[NET_CLIENT_DRIVER__MAX])(
    const Netdev *netdev,
    const char *name,
    NetClientState *peer, Error **errp) = {
        [NET_CLIENT_DRIVER_NIC]       = net_init_nic,
        [NET_CLIENT_DRIVER_TAP]       = net_init_tap,
};


static int net_client_init1(const Netdev *netdev, bool is_netdev, Error **errp)
{
    NetClientState *peer = NULL;
    NetClientState *nc;

    if (is_netdev) {
        if (netdev->type == NET_CLIENT_DRIVER_NIC ||
            !net_client_init_fun[netdev->type]) {
            error_setg(errp, "network backend '%s' is not compiled into this binary",
                       NetClientDriver_str(netdev->type));
            return -1;
        }
    } else {
        if (netdev->type == NET_CLIENT_DRIVER_NONE) {
            return 0; /* nothing to do */
        }
        if (!net_client_init_fun[netdev->type]) {
            error_setg(errp, "network backend '%s' is not compiled into this binary",
                       NetClientDriver_str(netdev->type));
            return -1;
        }
    }

    nc = qemu_find_netdev(netdev->id);
    if (nc) {
        error_setg(errp, "Duplicate ID '%s'", netdev->id);
        return -1;
    }

    if (net_client_init_fun[netdev->type](netdev, netdev->id, peer, errp) < 0) {
        if (errp && !*errp) {
            error_setg(errp, "Device '%s' could not be initialized",
                       NetClientDriver_str(netdev->type));
        }
        return -1;
    }

    if (is_netdev) {
        nc = qemu_find_netdev(netdev->id);
        assert(nc);
        nc->is_netdev = true;
    }

    return 0;
}

void show_netdevs(void)
{
    int idx;
    const char *available_netdevs[] = {
        "tap",
    };

    qemu_printf("Available netdev backend types:\n");
    for (idx = 0; idx < ARRAY_SIZE(available_netdevs); idx++) {
        qemu_printf("%s\n", available_netdevs[idx]);
    }
}

static int net_client_init(QemuOpts *opts, bool is_netdev, Error **errp)
{
    gchar **substrings = NULL;
    Netdev *object = NULL;
    int ret = -1;
    Visitor *v = opts_visitor_new(opts);

    const char *ip6_net = qemu_opt_get(opts, "ipv6-net");

    if (ip6_net) {
        char *prefix_addr;
        unsigned long prefix_len = 64; /* Default 64bit prefix length. */

        substrings = g_strsplit(ip6_net, "/", 2);
        if (!substrings || !substrings[0]) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "ipv6-net",
                       "a valid IPv6 prefix");
            goto out;
        }

        prefix_addr = substrings[0];

        if (substrings[1] &&
            qemu_strtoul(substrings[1], NULL, 10, &prefix_len))
        {
            error_setg(errp,
                       "parameter 'ipv6-net' expects a number after '/'");
            goto out;
        }

        qemu_opt_set(opts, "ipv6-prefix", prefix_addr, &error_abort);
        qemu_opt_set_number(opts, "ipv6-prefixlen", prefix_len,
                            &error_abort);
        qemu_opt_unset(opts, "ipv6-net");
    }

    if (!is_netdev && !qemu_opts_id(opts)) {
        qemu_opts_set_id(opts, id_generate(ID_NET));
    }

    if (visit_type_Netdev(v, NULL, &object, errp)) {
        ret = net_client_init1(object, is_netdev, errp);
    }

    qapi_free_Netdev(object);

out:
    g_strfreev(substrings);
    visit_free(v);
    return ret;
}

void netdev_add(QemuOpts *opts, Error **errp)
{
    net_client_init(opts, true, errp);
}

static void net_vm_change_state_handler(void *opaque, bool running,
                                        RunState state)
{
    NetClientState *nc;
    NetClientState *tmp;

    QTAILQ_FOREACH_SAFE(nc, &net_clients, next, tmp) {
        if (running) {
            if (nc->peer && qemu_can_send_packet(nc)) {
                qemu_flush_queued_packets(nc->peer);
            }
        } else {
            qemu_flush_or_purge_queued_packets(nc, true);
        }
    }
}

void net_cleanup(void)
{
    NetClientState *nc, **p = &QTAILQ_FIRST(&net_clients);

    while (*p) {
        nc = *p;
        if (nc->info->type == NET_CLIENT_DRIVER_NIC) {
            NICState *nic = qemu_get_nic(nc);

            if (nic->peer_deleted) {
                int queues = MAX(nic->conf->peers.queues, 1);

                for (int i = 0; i < queues; i++) {
                    nc = qemu_get_subqueue(nic, i);
                    qemu_cleanup_net_client(nc->peer, false);
                }
            }

            p = &QTAILQ_NEXT(nc, next);
        } else {
            qemu_del_net_client(nc);
        }
    }

    qemu_del_vm_change_state_handler(net_change_state_entry);
}

void net_check_clients(void)
{
    NetClientState *nc;
    int i;

    if (nic_model_help) {
        show_nic_models();
        exit(0);
    }

    QTAILQ_FOREACH(nc, &net_clients, next) {
        if (!nc->peer) {
            warn_report("%s %s has no peer",
                        nc->info->type == NET_CLIENT_DRIVER_NIC
                        ? "nic" : "netdev",
                        nc->name);
        }
    }

    for (i = 0; i < MAX_NICS; i++) {
        NICInfo *nd = &nd_table[i];
        if (nd->used && !nd->instantiated) {
            warn_report("requested NIC (%s, model %s) "
                        "was not created (not supported by this machine?)",
                        nd->name ? nd->name : "anonymous",
                        nd->model ? nd->model : "unspecified");
        }
    }
}

static int net_init_client(void *dummy, QemuOpts *opts, Error **errp)
{
    const char *model = qemu_opt_get(opts, "model");

    if (is_nic_model_help_option(model)) {
        return 0;
    }

    return net_client_init(opts, false, errp);
}

static int net_init_netdev(void *dummy, QemuOpts *opts, Error **errp)
{
    const char *type = qemu_opt_get(opts, "type");

    if (type && is_help_option(type)) {
        show_netdevs();
        exit(0);
    }
    return net_client_init(opts, true, errp);
}

static int net_param_nic(void *dummy, QemuOpts *opts, Error **errp)
{
    char *mac, *nd_id;
    int idx, ret;
    NICInfo *ni;
    const char *type;

    type = qemu_opt_get(opts, "type");
    if (type) {
        if (g_str_equal(type, "none")) {
            return 0;    /* Nothing to do, default_net is cleared in vl.c */
        }
        if (is_help_option(type)) {
            GPtrArray *nic_models = qemu_get_nic_models(TYPE_DEVICE);
            int i;
            show_netdevs();
            printf("\n");
            printf("Available NIC models "
                   "(use -nic model=help for a filtered list):\n");
            for (i = 0 ; nic_models->pdata[i]; i++) {
                printf("%s\n", (char *)nic_models->pdata[i]);
            }
            g_ptr_array_free(nic_models, true);
            exit(0);
        }
    }

    idx = nic_get_free_idx();
    if (idx == -1 || nb_nics >= MAX_NICS) {
        error_setg(errp, "no more on-board/default NIC slots available");
        return -1;
    }

    if (!type) {
        qemu_opt_set(opts, "type", "tap", &error_abort);
    }

    ni = &nd_table[idx];
    memset(ni, 0, sizeof(*ni));
    ni->model = qemu_opt_get_del(opts, "model");

    if (is_nic_model_help_option(ni->model)) {
        return 0;
    }

    nd_id = g_strdup(qemu_opts_id(opts));
    if (!nd_id) {
        nd_id = id_generate(ID_NET);
        qemu_opts_set_id(opts, nd_id);
    }

    mac = qemu_opt_get_del(opts, "mac");
    if (mac) {
        ret = net_parse_macaddr(ni->macaddr.a, mac);
        g_free(mac);
        if (ret) {
            error_setg(errp, "invalid syntax for ethernet address");
            goto out;
        }
        if (is_multicast_ether_addr(ni->macaddr.a)) {
            error_setg(errp, "NIC cannot have multicast MAC address");
            ret = -1;
            goto out;
        }
    }
    qemu_macaddr_default_if_unset(&ni->macaddr);

    ret = net_client_init(opts, true, errp);
    if (ret == 0) {
        ni->netdev = qemu_find_netdev(nd_id);
        ni->used = true;
        nb_nics++;
    }

out:
    g_free(nd_id);
    return ret;
}

static void netdev_init_modern(void)
{
    while (!QSIMPLEQ_EMPTY(&nd_queue)) {
        NetdevQueueEntry *nd = QSIMPLEQ_FIRST(&nd_queue);

        QSIMPLEQ_REMOVE_HEAD(&nd_queue, entry);
        loc_push_restore(&nd->loc);
        net_client_init1(nd->nd, true, &error_fatal);
        loc_pop(&nd->loc);
        qapi_free_Netdev(nd->nd);
        g_free(nd);
    }
}

void net_init_clients(void)
{
    net_change_state_entry =
        qemu_add_vm_change_state_handler(net_vm_change_state_handler, NULL);

    QTAILQ_INIT(&net_clients);

    netdev_init_modern();

    qemu_opts_foreach(qemu_find_opts("netdev"), net_init_netdev, NULL,
                      &error_fatal);

    qemu_opts_foreach(qemu_find_opts("nic"), net_param_nic, NULL,
                      &error_fatal);

    qemu_opts_foreach(qemu_find_opts("net"), net_init_client, NULL,
                      &error_fatal);
}

bool netdev_is_modern(const char *optstr)
{
    QemuOpts *opts;
    bool is_modern;
    const char *type;
    static QemuOptsList dummy_opts = {
        .name = "netdev",
        .implied_opt_name = "type",
        .head = QTAILQ_HEAD_INITIALIZER(dummy_opts.head),
        .desc = { { } },
    };

    if (optstr[0] == '{') {
        return true;
    }

    opts = qemu_opts_create(&dummy_opts, NULL, false, &error_abort);
    qemu_opts_do_parse(opts, optstr, dummy_opts.implied_opt_name,
                       &error_abort);
    type = qemu_opt_get(opts, "type");
    is_modern = !g_strcmp0(type, "stream") || !g_strcmp0(type, "dgram");

    qemu_opts_reset(&dummy_opts);

    return is_modern;
}

void netdev_parse_modern(const char *optstr)
{
    Visitor *v;
    NetdevQueueEntry *nd;

    v = qobject_input_visitor_new_str(optstr, "type", &error_fatal);
    nd = g_new(NetdevQueueEntry, 1);
    visit_type_Netdev(v, NULL, &nd->nd, &error_fatal);
    visit_free(v);
    loc_save(&nd->loc);

    QSIMPLEQ_INSERT_TAIL(&nd_queue, nd, entry);
}

void net_client_parse(QemuOptsList *opts_list, const char *optstr)
{
    if (!qemu_opts_parse_noisily(opts_list, optstr, true)) {
        exit(1);
    }
}

uint32_t net_crc32(const uint8_t *p, int len)
{
    uint32_t crc;
    int carry, i, j;
    uint8_t b;

    crc = 0xffffffff;
    for (i = 0; i < len; i++) {
        b = *p++;
        for (j = 0; j < 8; j++) {
            carry = ((crc & 0x80000000L) ? 1 : 0) ^ (b & 0x01);
            crc <<= 1;
            b >>= 1;
            if (carry) {
                crc = ((crc ^ POLYNOMIAL_BE) | carry);
            }
        }
    }

    return crc;
}

uint32_t net_crc32_le(const uint8_t *p, int len)
{
    uint32_t crc;
    int carry, i, j;
    uint8_t b;

    crc = 0xffffffff;
    for (i = 0; i < len; i++) {
        b = *p++;
        for (j = 0; j < 8; j++) {
            carry = (crc & 0x1) ^ (b & 0x01);
            crc >>= 1;
            b >>= 1;
            if (carry) {
                crc ^= POLYNOMIAL_LE;
            }
        }
    }

    return crc;
}

QemuOptsList qemu_netdev_opts = {
    .name = "netdev",
    .implied_opt_name = "type",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_netdev_opts.head),
    .desc = {
        { /* end of list */ }
    },
};

QemuOptsList qemu_nic_opts = {
    .name = "nic",
    .implied_opt_name = "type",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_nic_opts.head),
    .desc = {
        { /* end of list */ }
    },
};

QemuOptsList qemu_net_opts = {
    .name = "net",
    .implied_opt_name = "type",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_net_opts.head),
    .desc = {
        { /* end of list */ }
    },
};

void net_socket_rs_init(SocketReadState *rs,
                        SocketReadStateFinalize *finalize,
                        bool vnet_hdr)
{
    rs->state = 0;
    rs->vnet_hdr = vnet_hdr;
    rs->index = 0;
    rs->packet_len = 0;
    rs->vnet_hdr_len = 0;
    memset(rs->buf, 0, sizeof(rs->buf));
    rs->finalize = finalize;
}

int net_fill_rstate(SocketReadState *rs, const uint8_t *buf, int size)
{
    unsigned int l;

    while (size > 0) {
        switch (rs->state) {
        case 0:
            l = 4 - rs->index;
            if (l > size) {
                l = size;
            }
            memcpy(rs->buf + rs->index, buf, l);
            buf += l;
            size -= l;
            rs->index += l;
            if (rs->index == 4) {
                rs->packet_len = ntohl(*(uint32_t *)rs->buf);
                rs->index = 0;
                if (rs->vnet_hdr) {
                    rs->state = 1;
                } else {
                    rs->state = 2;
                    rs->vnet_hdr_len = 0;
                }
            }
            break;
        case 1:
            l = 4 - rs->index;
            if (l > size) {
                l = size;
            }
            memcpy(rs->buf + rs->index, buf, l);
            buf += l;
            size -= l;
            rs->index += l;
            if (rs->index == 4) {
                rs->vnet_hdr_len = ntohl(*(uint32_t *)rs->buf);
                rs->index = 0;
                rs->state = 2;
            }
            break;
        case 2:
            l = rs->packet_len - rs->index;
            if (l > size) {
                l = size;
            }
            if (rs->index + l <= sizeof(rs->buf)) {
                memcpy(rs->buf + rs->index, buf, l);
            } else {
                fprintf(stderr, "serious error: oversized packet received,"
                    "connection terminated.\n");
                rs->index = rs->state = 0;
                return -1;
            }

            rs->index += l;
            buf += l;
            size -= l;
            if (rs->index >= rs->packet_len) {
                rs->index = 0;
                rs->state = 0;
                assert(rs->finalize);
                rs->finalize(rs);
            }
            break;
        }
    }

    assert(size == 0);
    return 0;
}
