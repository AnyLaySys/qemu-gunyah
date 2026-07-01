
#ifndef VIRTIO_DMABUF_H
#define VIRTIO_DMABUF_H

#include "qemu/uuid.h"

typedef enum SharedObjectType {
    TYPE_INVALID = 0,
    TYPE_DMABUF,
} SharedObjectType;

typedef struct VirtioSharedObject {
    SharedObjectType type;
    gpointer value;
} VirtioSharedObject;

bool virtio_add_dmabuf(QemuUUID *uuid, int dmabuf_fd);

bool virtio_remove_resource(const QemuUUID *uuid);

int virtio_lookup_dmabuf(const QemuUUID *uuid);

SharedObjectType virtio_object_type(const QemuUUID *uuid);

void virtio_free_resources(void);

#endif /* VIRTIO_DMABUF_H */
