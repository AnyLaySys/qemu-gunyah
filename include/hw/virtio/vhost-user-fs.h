
#ifndef QEMU_VHOST_USER_FS_H
#define QEMU_VHOST_USER_FS_H

#include "hw/virtio/virtio.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define TYPE_VHOST_USER_FS "vhost-user-fs-device"
OBJECT_DECLARE_SIMPLE_TYPE(VHostUserFS, VHOST_USER_FS)

typedef struct {
    CharBackend chardev;
    char *tag;
    uint16_t num_request_queues;
    uint16_t queue_size;
} VHostUserFSConf;

struct VHostUserFS {
    VirtIODevice parent;
    VHostUserFSConf conf;
    struct vhost_virtqueue *vhost_vqs;
    struct vhost_dev vhost_dev;
    VhostUserState vhost_user;
    VirtQueue **req_vqs;
    VirtQueue *hiprio_vq;
    int32_t bootindex;

};

#endif /* QEMU_VHOST_USER_FS_H */
