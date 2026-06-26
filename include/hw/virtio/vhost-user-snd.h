
#ifndef QEMU_VHOST_USER_SND_H
#define QEMU_VHOST_USER_SND_H

#include "hw/virtio/virtio.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"
#include "hw/virtio/vhost-user-base.h"

#define TYPE_VHOST_USER_SND "vhost-user-snd"
OBJECT_DECLARE_SIMPLE_TYPE(VHostUserSound, VHOST_USER_SND)

struct VHostUserSound {
    VHostUserBase parent_obj;
};

#endif /* QEMU_VHOST_USER_SND_H */
