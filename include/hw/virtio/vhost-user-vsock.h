
#ifndef QEMU_VHOST_USER_VSOCK_H
#define QEMU_VHOST_USER_VSOCK_H

#include "hw/virtio/vhost-vsock-common.h"
#include "hw/virtio/vhost-user.h"
#include "standard-headers/linux/virtio_vsock.h"
#include "qom/object.h"

#define TYPE_VHOST_USER_VSOCK "vhost-user-vsock-device"
OBJECT_DECLARE_SIMPLE_TYPE(VHostUserVSock, VHOST_USER_VSOCK)

typedef struct {
    CharBackend chardev;
} VHostUserVSockConf;

struct VHostUserVSock {
    VHostVSockCommon parent;
    VhostUserState vhost_user;
    VHostUserVSockConf conf;
    struct virtio_vsock_config vsockcfg;

};

#endif /* QEMU_VHOST_USER_VSOCK_H */
