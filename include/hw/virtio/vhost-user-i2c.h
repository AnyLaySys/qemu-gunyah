
#ifndef QEMU_VHOST_USER_I2C_H
#define QEMU_VHOST_USER_I2C_H

#include "hw/virtio/virtio.h"
#include "hw/virtio/vhost.h"
#include "hw/virtio/vhost-user.h"
#include "hw/virtio/vhost-user-base.h"

#define TYPE_VHOST_USER_I2C "vhost-user-i2c-device"

OBJECT_DECLARE_SIMPLE_TYPE(VHostUserI2C, VHOST_USER_I2C)

struct VHostUserI2C {
    VHostUserBase parent_obj;
};

#endif /* QEMU_VHOST_USER_I2C_H */
