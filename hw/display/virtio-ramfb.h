#ifndef VIRTIO_RAMFB_H
#define VIRTIO_RAMFB_H

#include "hw/virtio/virtio-gpu-pci.h"
#include "hw/display/ramfb.h"
#include "qom/object.h"

/*
 * virtio-ramfb-base: This extends VirtioPCIProxy.
 */
#define TYPE_VIRTIO_RAMFB_BASE "virtio-ramfb-base"
OBJECT_DECLARE_TYPE(VirtIORAMFBBase, VirtIORAMFBBaseClass,
                    VIRTIO_RAMFB_BASE)

struct VirtIORAMFBBase {
    VirtIOPCIProxy parent_obj;

    VirtIOGPUBase *vgpu;
    RAMFBState    *ramfb;
};

struct VirtIORAMFBBaseClass {
    VirtioPCIClass parent_class;

    ResettablePhases parent_phases;
};

#endif /* VIRTIO_RAMFB_H */
