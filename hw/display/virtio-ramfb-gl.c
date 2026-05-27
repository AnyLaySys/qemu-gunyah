#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-gpu.h"
#include "hw/display/vga.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "virtio-ramfb.h"
#include "qom/object.h"

#define TYPE_VIRTIO_RAMFB_GL "virtio-ramfb-gl"

typedef struct VirtIORAMFBGL VirtIORAMFBGL;
DECLARE_INSTANCE_CHECKER(VirtIORAMFBGL, VIRTIO_RAMFB_GL,
                         TYPE_VIRTIO_RAMFB_GL)

struct VirtIORAMFBGL {
    VirtIORAMFBBase parent_obj;

    VirtIOGPUGL   vdev;
};

static void virtio_ramfb_gl_inst_initfn(Object *obj)
{
    VirtIORAMFBGL *dev = VIRTIO_RAMFB_GL(obj);
    
    virtio_instance_init_common(obj, &dev->vdev, sizeof(dev->vdev),
                                TYPE_VIRTIO_GPU_GL);
    VIRTIO_RAMFB_BASE(dev)->vgpu = VIRTIO_GPU_BASE(&dev->vdev);
}


static VirtioPCIDeviceTypeInfo virtio_ramfb_gl_info = {
    .generic_name  = TYPE_VIRTIO_RAMFB_GL,
    .parent        = TYPE_VIRTIO_RAMFB_BASE,
    .instance_size = sizeof(VirtIORAMFBGL),
    .instance_init = virtio_ramfb_gl_inst_initfn,
};
module_obj(TYPE_VIRTIO_RAMFB_GL);

static void virtio_ramfb_register_types(void)
{
    if (have_vga) {
        virtio_pci_types_register(&virtio_ramfb_gl_info);
    }
}

type_init(virtio_ramfb_register_types)

module_dep("hw-display-virtio-ramfb");
