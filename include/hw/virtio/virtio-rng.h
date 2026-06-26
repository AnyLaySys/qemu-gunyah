
#ifndef QEMU_VIRTIO_RNG_H
#define QEMU_VIRTIO_RNG_H

#include "hw/virtio/virtio.h"
#include "system/rng.h"
#include "standard-headers/linux/virtio_rng.h"
#include "qom/object.h"

#define TYPE_VIRTIO_RNG "virtio-rng-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIORNG, VIRTIO_RNG)
#define VIRTIO_RNG_GET_PARENT_CLASS(obj) \
        OBJECT_GET_PARENT_CLASS(obj, TYPE_VIRTIO_RNG)

struct VirtIORNGConf {
    RngBackend *rng;
    uint64_t max_bytes;
    uint32_t period_ms;
};

struct VirtIORNG {
    VirtIODevice parent_obj;

    VirtQueue *vq;

    VirtIORNGConf conf;

    RngBackend *rng;

    QEMUTimer *rate_limit_timer;
    int64_t quota_remaining;
    bool activate_timer;

    VMChangeStateEntry *vmstate;
};

#endif
