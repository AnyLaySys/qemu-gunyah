

#include "qemu/osdep.h"

#include "hw/virtio/virtio-gpu.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/display/edid.h"
#include "trace.h"

void
virtio_gpu_base_reset(VirtIOGPUBase *g)
{
    int i;

    g->enable = 0;

    for (i = 0; i < g->conf.max_outputs; i++) {
        g->scanout[i].resource_id = 0;
        g->scanout[i].width = 0;
        g->scanout[i].height = 0;
        g->scanout[i].x = 0;
        g->scanout[i].y = 0;
        g->scanout[i].ds = NULL;
    }
}

void
virtio_gpu_base_fill_display_info(VirtIOGPUBase *g,
                                  struct virtio_gpu_resp_display_info *dpy_info)
{
    int i;

    for (i = 0; i < g->conf.max_outputs; i++) {
        if (g->enabled_output_bitmask & (1 << i)) {
            dpy_info->pmodes[i].enabled = 1;
            dpy_info->pmodes[i].r.width = cpu_to_le32(g->req_state[i].width);
            dpy_info->pmodes[i].r.height = cpu_to_le32(g->req_state[i].height);
        }
    }
}

void
virtio_gpu_base_generate_edid(VirtIOGPUBase *g, int scanout,
                              struct virtio_gpu_resp_edid *edid)
{
    qemu_edid_info info = {
        .width_mm = g->req_state[scanout].width_mm,
        .height_mm = g->req_state[scanout].height_mm,
        .prefx = g->req_state[scanout].width,
        .prefy = g->req_state[scanout].height,
        .refresh_rate = g->req_state[scanout].refresh_rate,
    };

    edid->size = cpu_to_le32(sizeof(edid->edid));
    qemu_edid_generate(edid->edid, sizeof(edid->edid), &info);
}

static void virtio_gpu_notify_event(VirtIOGPUBase *g, uint32_t event_type)
{
    g->virtio_config.events_read |= event_type;
    virtio_notify_config(&g->parent_obj);
}

static void virtio_gpu_ui_info(void *opaque, uint32_t idx, QemuUIInfo *info)
{
    VirtIOGPUBase *g = opaque;

    if (idx >= g->conf.max_outputs) {
        return;
    }

    g->req_state[idx].x = info->xoff;
    g->req_state[idx].y = info->yoff;
    g->req_state[idx].refresh_rate = info->refresh_rate;
    g->req_state[idx].width = info->width;
    g->req_state[idx].height = info->height;
    g->req_state[idx].width_mm = info->width_mm;
    g->req_state[idx].height_mm = info->height_mm;

    if (info->width && info->height) {
        g->enabled_output_bitmask |= (1 << idx);
    } else {
        g->enabled_output_bitmask &= ~(1 << idx);
    }

    
    virtio_gpu_notify_event(g, VIRTIO_GPU_EVENT_DISPLAY);
    return;
}

static int
virtio_gpu_get_flags(void *opaque)
{
    VirtIOGPUBase *g = opaque;
    int flags = GRAPHIC_FLAGS_NONE;

    if (virtio_gpu_dmabuf_enabled(g->conf)) {
        flags |= GRAPHIC_FLAGS_DMABUF;
    }

    return flags;
}

static const GraphicHwOps virtio_gpu_ops = {
    .get_flags = virtio_gpu_get_flags,
    .ui_info = virtio_gpu_ui_info,
};

bool
virtio_gpu_base_device_realize(DeviceState *qdev,
                               VirtIOHandleOutput ctrl_cb,
                               VirtIOHandleOutput cursor_cb,
                               Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(qdev);
    VirtIOGPUBase *g = VIRTIO_GPU_BASE(qdev);
    int i;

    if (g->conf.max_outputs > VIRTIO_GPU_MAX_SCANOUTS) {
        error_setg(errp, "invalid max_outputs > %d", VIRTIO_GPU_MAX_SCANOUTS);
        return false;
    }

    g->virtio_config.num_scanouts = cpu_to_le32(g->conf.max_outputs);
    virtio_init(VIRTIO_DEVICE(g), VIRTIO_ID_GPU,
                sizeof(struct virtio_gpu_config));

    virtio_add_queue(vdev, 64, ctrl_cb);
    virtio_add_queue(vdev, 16, cursor_cb);

    g->enabled_output_bitmask = 1;

    g->req_state[0].width = g->conf.xres;
    g->req_state[0].height = g->conf.yres;

    g->hw_ops = &virtio_gpu_ops;
    for (i = 0; i < g->conf.max_outputs; i++) {
        g->scanout[i].con =
            graphic_console_init(DEVICE(g), i, &virtio_gpu_ops, g);
    }

    return true;
}

static uint64_t
virtio_gpu_base_get_features(VirtIODevice *vdev, uint64_t features,
                             Error **errp)
{
    VirtIOGPUBase *g = VIRTIO_GPU_BASE(vdev);

    if (virtio_gpu_edid_enabled(g->conf)) {
        features |= (1 << VIRTIO_GPU_F_EDID);
    }
    if (virtio_gpu_blob_enabled(g->conf)) {
        features |= (1 << VIRTIO_GPU_F_RESOURCE_BLOB);
    }
    if (virtio_gpu_context_init_enabled(g->conf)) {
        features |= (1 << VIRTIO_GPU_F_CONTEXT_INIT);
    }
    if (virtio_gpu_resource_uuid_enabled(g->conf)) {
        features |= (1 << VIRTIO_GPU_F_RESOURCE_UUID);
    }
    if (virtio_gpu_virgl_enabled(g->conf)) {
        features |= (1 << VIRTIO_GPU_F_VIRGL);
    }

    return features;
}

void
virtio_gpu_base_device_unrealize(DeviceState *qdev)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(qdev);

    virtio_del_queue(vdev, 0);
    virtio_del_queue(vdev, 1);
    virtio_cleanup(vdev);
}

static void
virtio_gpu_base_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);

    vdc->unrealize = virtio_gpu_base_device_unrealize;
    vdc->get_features = virtio_gpu_base_get_features;

    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->hotpluggable = false;
}

static const TypeInfo virtio_gpu_base_info = {
    .name = TYPE_VIRTIO_GPU_BASE,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOGPUBase),
    .class_size = sizeof(VirtIOGPUBaseClass),
    .class_init = virtio_gpu_base_class_init,
    .abstract = true
};
module_obj(TYPE_VIRTIO_GPU_BASE);
module_kconfig(VIRTIO_GPU);

static void
virtio_register_types(void)
{
    type_register_static(&virtio_gpu_base_info);
}

type_init(virtio_register_types)

QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_ctrl_hdr)                != 24);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_update_cursor)           != 56);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_unref)          != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_create_2d)      != 40);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_set_scanout)             != 48);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_flush)          != 48);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_transfer_to_host_2d)     != 56);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_mem_entry)               != 16);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_attach_backing) != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_detach_backing) != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resp_display_info)       != 408);

QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_transfer_host_3d)        != 72);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resource_create_3d)      != 72);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_ctx_create)              != 96);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_ctx_destroy)             != 24);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_ctx_resource)            != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_cmd_submit)              != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_get_capset_info)         != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resp_capset_info)        != 40);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_get_capset)              != 32);
QEMU_BUILD_BUG_ON(sizeof(struct virtio_gpu_resp_capset)             != 24);
