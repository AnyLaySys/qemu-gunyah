
#ifndef QEMU_VHOST_VSOCK_H
#define QEMU_VHOST_VSOCK_H

#include "hw/virtio/vhost-vsock-common.h"
#include "qom/object.h"

#define TYPE_VHOST_VSOCK "vhost-vsock-device"
OBJECT_DECLARE_SIMPLE_TYPE(VHostVSock, VHOST_VSOCK)

typedef struct {
    uint64_t guest_cid;
    char *vhostfd;
} VHostVSockConf;

struct VHostVSock {
    VHostVSockCommon parent;
    VHostVSockConf conf;

};

#endif /* QEMU_VHOST_VSOCK_H */
