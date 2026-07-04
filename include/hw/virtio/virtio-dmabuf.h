
#ifndef VIRTIO_DMABUF_H
#define VIRTIO_DMABUF_H

#include "qemu/uuid.h"
#include "vhost.h"

typedef enum SharedObjectType {
    TYPE_INVALID = 0,
    TYPE_DMABUF,
    TYPE_VHOST_DEV,
} SharedObjectType;

typedef struct VirtioSharedObject {
    SharedObjectType type;
    gpointer value;
} VirtioSharedObject;

bool virtio_add_dmabuf(QemuUUID *uuid, int dmabuf_fd);

bool virtio_add_vhost_device(QemuUUID *uuid, struct vhost_dev *dev);

bool virtio_remove_resource(const QemuUUID *uuid);

int virtio_lookup_dmabuf(const QemuUUID *uuid);

struct vhost_dev *virtio_lookup_vhost_device(const QemuUUID *uuid);

SharedObjectType virtio_object_type(const QemuUUID *uuid);

void virtio_free_resources(void);

#endif /* VIRTIO_DMABUF_H */
