
#ifndef VHOST_USER_BLK_H
#define VHOST_USER_BLK_H

#include "standard-headers/linux/virtio_blk.h"
#include "hw/block/block.h"
#include "chardev/char-fe.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"
#include "qom/object.h"

#define TYPE_VHOST_USER_BLK "vhost-user-blk"
OBJECT_DECLARE_SIMPLE_TYPE(VHostUserBlk, VHOST_USER_BLK)

#define VHOST_USER_BLK_AUTO_NUM_QUEUES UINT16_MAX

struct VHostUserBlk {
    VirtIODevice parent_obj;
    CharBackend chardev;
    int32_t bootindex;
    struct virtio_blk_config blkcfg;
    uint16_t num_queues;
    uint32_t queue_size;
    struct vhost_dev dev;
    struct vhost_inflight *inflight;
    VhostUserState vhost_user;
    struct vhost_virtqueue *vhost_vqs;
    VirtQueue **virtqs;

    bool connected;
    bool started_vu;
};

#endif
