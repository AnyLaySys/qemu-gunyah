
#ifndef HW_VIRTIO_GPU_PCI_H
#define HW_VIRTIO_GPU_PCI_H

#include "hw/virtio/virtio-pci.h"
#include "hw/virtio/virtio-gpu.h"
#include "qom/object.h"

#define TYPE_VIRTIO_GPU_PCI_BASE "virtio-gpu-pci-base"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOGPUPCIBase, VIRTIO_GPU_PCI_BASE)

struct VirtIOGPUPCIBase {
    VirtIOPCIProxy parent_obj;
    VirtIOGPUBase *vgpu;
};

#define DEFINE_VIRTIO_GPU_PCI_PROPERTIES(_state)                \
    DEFINE_PROP_BIT("ioeventfd", _state, flags,                 \
                    VIRTIO_PCI_FLAG_USE_IOEVENTFD_BIT, false),  \
        DEFINE_PROP_UINT32("vectors", _state, nvectors, 3)

#endif /* HW_VIRTIO_GPU_PCI_H */
