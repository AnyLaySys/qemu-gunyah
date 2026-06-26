
#ifndef HW_VIRTIO_PMEM_H
#define HW_VIRTIO_PMEM_H

#include "hw/virtio/virtio.h"
#include "qapi/qapi-types-machine.h"
#include "qom/object.h"

#define TYPE_VIRTIO_PMEM "virtio-pmem"

OBJECT_DECLARE_TYPE(VirtIOPMEM, VirtIOPMEMClass,
                    VIRTIO_PMEM)

#define VIRTIO_PMEM_ADDR_PROP "memaddr"
#define VIRTIO_PMEM_MEMDEV_PROP "memdev"

struct VirtIOPMEM {
    VirtIODevice parent_obj;

    VirtQueue *rq_vq;
    uint64_t start;
    HostMemoryBackend *memdev;
};

struct VirtIOPMEMClass {
    VirtIODevice parent;

    void (*fill_device_info)(const VirtIOPMEM *pmem, VirtioPMEMDeviceInfo *vi);
    MemoryRegion *(*get_memory_region)(VirtIOPMEM *pmem, Error **errp);
};

#endif
