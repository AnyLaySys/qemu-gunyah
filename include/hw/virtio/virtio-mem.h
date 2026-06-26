
#ifndef HW_VIRTIO_MEM_H
#define HW_VIRTIO_MEM_H

#include "standard-headers/linux/virtio_mem.h"
#include "hw/resettable.h"
#include "hw/virtio/virtio.h"
#include "qapi/qapi-types-misc.h"
#include "system/hostmem.h"
#include "qom/object.h"

#define TYPE_VIRTIO_MEM "virtio-mem"

OBJECT_DECLARE_TYPE(VirtIOMEM, VirtIOMEMClass,
                    VIRTIO_MEM)

#define TYPE_VIRTIO_MEM_SYSTEM_RESET "virtio-mem-system-reset"

OBJECT_DECLARE_SIMPLE_TYPE(VirtioMemSystemReset, VIRTIO_MEM_SYSTEM_RESET)

#define VIRTIO_MEM_MEMDEV_PROP "memdev"
#define VIRTIO_MEM_NODE_PROP "node"
#define VIRTIO_MEM_SIZE_PROP "size"
#define VIRTIO_MEM_REQUESTED_SIZE_PROP "requested-size"
#define VIRTIO_MEM_BLOCK_SIZE_PROP "block-size"
#define VIRTIO_MEM_ADDR_PROP "memaddr"
#define VIRTIO_MEM_UNPLUGGED_INACCESSIBLE_PROP "unplugged-inaccessible"
#define VIRTIO_MEM_EARLY_MIGRATION_PROP "x-early-migration"
#define VIRTIO_MEM_PREALLOC_PROP "prealloc"
#define VIRTIO_MEM_DYNAMIC_MEMSLOTS_PROP "dynamic-memslots"

struct VirtIOMEM {
    VirtIODevice parent_obj;

    VirtQueue *vq;

    int32_t bitmap_size;
    unsigned long *bitmap;

    MemoryRegion *mr;

    MemoryRegion *memslots;

    uint16_t nb_memslots;

    uint64_t memslot_size;

    HostMemoryBackend *memdev;

    uint32_t node;

    uint64_t addr;

    uint64_t usable_region_size;

    uint64_t size;

    uint64_t requested_size;

    uint64_t block_size;

    OnOffAuto unplugged_inaccessible;

    bool prealloc;

    bool early_migration;

    bool dynamic_memslots;

    NotifierList size_change_notifiers;

    QLIST_HEAD(, RamDiscardListener) rdl_list;

    VirtioMemSystemReset *system_reset;
};

struct VirtioMemSystemReset {
    Object parent;

    ResettableState reset_state;
    VirtIOMEM *vmem;
};

struct VirtIOMEMClass {
    VirtIODevice parent;

    void (*fill_device_info)(const VirtIOMEM *vmen, VirtioMEMDeviceInfo *vi);
    MemoryRegion *(*get_memory_region)(VirtIOMEM *vmem, Error **errp);
    void (*decide_memslots)(VirtIOMEM *vmem, unsigned int limit);
    unsigned int (*get_memslots)(VirtIOMEM *vmem);
    void (*add_size_change_notifier)(VirtIOMEM *vmem, Notifier *notifier);
    void (*remove_size_change_notifier)(VirtIOMEM *vmem, Notifier *notifier);
    void (*unplug_request_check)(VirtIOMEM *vmem, Error **errp);
};

#endif
