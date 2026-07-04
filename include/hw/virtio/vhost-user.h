
#ifndef HW_VIRTIO_VHOST_USER_H
#define HW_VIRTIO_VHOST_USER_H

#include "chardev/char-fe.h"
#include "hw/virtio/virtio.h"

enum VhostUserProtocolFeature {
    VHOST_USER_PROTOCOL_F_MQ = 0,
    VHOST_USER_PROTOCOL_F_LOG_SHMFD = 1,
    VHOST_USER_PROTOCOL_F_RARP = 2,
    VHOST_USER_PROTOCOL_F_REPLY_ACK = 3,
    VHOST_USER_PROTOCOL_F_NET_MTU = 4,
    VHOST_USER_PROTOCOL_F_BACKEND_REQ = 5,
    VHOST_USER_PROTOCOL_F_CROSS_ENDIAN = 6,
    VHOST_USER_PROTOCOL_F_CRYPTO_SESSION = 7,
    VHOST_USER_PROTOCOL_F_PAGEFAULT = 8,
    VHOST_USER_PROTOCOL_F_CONFIG = 9,
    VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD = 10,
    VHOST_USER_PROTOCOL_F_HOST_NOTIFIER = 11,
    VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD = 12,
    VHOST_USER_PROTOCOL_F_RESET_DEVICE = 13,
    VHOST_USER_PROTOCOL_F_INBAND_NOTIFICATIONS = 14,
    VHOST_USER_PROTOCOL_F_CONFIGURE_MEM_SLOTS = 15,
    VHOST_USER_PROTOCOL_F_STATUS = 16,
    VHOST_USER_PROTOCOL_F_SHARED_OBJECT = 18,
    VHOST_USER_PROTOCOL_F_DEVICE_STATE = 19,
    VHOST_USER_PROTOCOL_F_MAX
};

typedef struct VhostUserHostNotifier {
    struct rcu_head rcu;
    MemoryRegion mr;
    void *addr;
    void *unmap_addr;
    int idx;
    bool destroy;
} VhostUserHostNotifier;

typedef struct VhostUserState {
    CharBackend *chr;
    GPtrArray *notifiers;
    int memory_slots;
    bool supports_config;
} VhostUserState;

bool vhost_user_init(VhostUserState *user, CharBackend *chr, Error **errp);

void vhost_user_cleanup(VhostUserState *user);

typedef void (*vu_async_close_fn)(DeviceState *cb);

void vhost_user_async_close(DeviceState *d,
                            CharBackend *chardev, struct vhost_dev *vhost,
                            vu_async_close_fn cb);

#endif
