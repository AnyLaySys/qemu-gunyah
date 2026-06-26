
#ifndef QEMU_VHOST_USER_BASE_H
#define QEMU_VHOST_USER_BASE_H

#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"

#define TYPE_VHOST_USER_BASE "vhost-user-base"

OBJECT_DECLARE_TYPE(VHostUserBase, VHostUserBaseClass, VHOST_USER_BASE)

struct VHostUserBase {
    VirtIODevice parent_obj;

    CharBackend chardev;
    uint16_t virtio_id;
    uint32_t num_vqs;
    uint32_t vq_size; /* can't exceed VIRTIO_QUEUE_MAX */
    uint32_t config_size;
    VhostUserState vhost_user;
    struct vhost_virtqueue *vhost_vq;
    struct vhost_dev vhost_dev;
    GPtrArray *vqs;
    bool connected;
};

struct VHostUserBaseClass {
    VirtioDeviceClass parent_class;

    DeviceRealize parent_realize;
};


#define TYPE_VHOST_USER_DEVICE "vhost-user-device"

#endif /* QEMU_VHOST_USER_BASE_H */
