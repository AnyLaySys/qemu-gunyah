
#ifndef MEMORY_DEVICE_H
#define MEMORY_DEVICE_H

#include "hw/qdev-core.h"
#include "qapi/qapi-types-machine.h"
#include "qom/object.h"

#define TYPE_MEMORY_DEVICE "memory-device"

typedef struct MemoryDeviceClass MemoryDeviceClass;
DECLARE_CLASS_CHECKERS(MemoryDeviceClass, MEMORY_DEVICE,
                       TYPE_MEMORY_DEVICE)
#define MEMORY_DEVICE(obj) \
     INTERFACE_CHECK(MemoryDeviceState, (obj), TYPE_MEMORY_DEVICE)

typedef struct MemoryDeviceState MemoryDeviceState;

struct MemoryDeviceClass {
    InterfaceClass parent_class;

    uint64_t (*get_addr)(const MemoryDeviceState *md);

    void (*set_addr)(MemoryDeviceState *md, uint64_t addr, Error **errp);

    uint64_t (*get_plugged_size)(const MemoryDeviceState *md, Error **errp);

    MemoryRegion *(*get_memory_region)(MemoryDeviceState *md, Error **errp);

    void (*decide_memslots)(MemoryDeviceState *md, unsigned int limit);

    unsigned int (*get_memslots)(MemoryDeviceState *md);

    uint64_t (*get_min_alignment)(const MemoryDeviceState *md);

    void (*fill_device_info)(const MemoryDeviceState *md,
                             MemoryDeviceInfo *info);
};

#define MEMORY_DEVICES_SOFT_MEMSLOT_LIMIT 256
#define MEMORY_DEVICES_SAFE_MAX_MEMSLOTS 509

MemoryDeviceInfoList *qmp_memory_device_list(void);
uint64_t get_plugged_memory_size(void);
unsigned int memory_devices_get_reserved_memslots(void);
bool memory_devices_memslot_auto_decision_active(void);
void memory_device_pre_plug(MemoryDeviceState *md, MachineState *ms,
                            Error **errp);
void memory_device_plug(MemoryDeviceState *md, MachineState *ms);
void memory_device_unplug(MemoryDeviceState *md, MachineState *ms);
uint64_t memory_device_get_region_size(const MemoryDeviceState *md,
                                       Error **errp);

#endif
