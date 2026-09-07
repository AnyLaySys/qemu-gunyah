#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "hw/qdev-core.h"
#include "hw/nvram/fw_cfg.h"
#include "ui/console.h"
#include "exec/address-spaces.h"

#define TYPE_RAMFB "ramfb"
#define RAMFB_FORMAT 0x34325258
#define RAMFB_MMIO_BASE 0x0f000000
#define RAMFB_MMIO_SIZE 0x01000000
OBJECT_DECLARE_SIMPLE_TYPE(RAMFBState, RAMFB)

typedef struct QEMU_PACKED RAMFBConfig {
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} RAMFBConfig;

struct RAMFBState {
    DeviceState parent_obj;
    QemuConsole *con;
    DisplaySurface *surface;
    RAMFBConfig config;
    MemoryRegion memory;
};

static void ramfb_config_write(void *opaque, off_t offset, size_t len)
{
    RAMFBState *s = opaque;
    uint32_t width = be32_to_cpu(s->config.width);
    uint32_t height = be32_to_cpu(s->config.height);
    uint32_t stride = be32_to_cpu(s->config.stride);
    hwaddr addr = be64_to_cpu(s->config.addr);
    hwaddr line_size = (hwaddr)width * 4;
    hwaddr map_size = (hwaddr)stride * height;
    uint8_t *data;

    if (be32_to_cpu(s->config.fourcc) != RAMFB_FORMAT ||
        width < 16 || width > 16384 ||
        height < 16 || height > 16384 || stride != line_size ||
        addr < RAMFB_MMIO_BASE ||
        map_size > RAMFB_MMIO_SIZE ||
        addr - RAMFB_MMIO_BASE > RAMFB_MMIO_SIZE - map_size) {
        return;
    }

    data = memory_region_get_ram_ptr(&s->memory) + addr - RAMFB_MMIO_BASE;
    qemu_free_displaysurface(s->surface);
    s->surface = qemu_create_displaysurface_from(width, height,
                                                 PIXMAN_LE_x8r8g8b8,
                                                 stride, data);
}

static void ramfb_update(void *opaque)
{
    RAMFBState *s = opaque;

    if (s->surface) {
        dpy_gfx_replace_surface(s->con, s->surface);
        s->surface = NULL;
    }
    dpy_gfx_update_full(s->con);
}

static const GraphicHwOps ramfb_ops = {
    .gfx_update = ramfb_update,
};

static void ramfb_realize(DeviceState *dev, Error **errp)
{
    RAMFBState *s = RAMFB(dev);
    FWCfgState *fw_cfg = fw_cfg_find();

    if (!fw_cfg) {
        error_setg(errp, "ramfb requires fw_cfg");
        return;
    }

    if (!memory_region_init_ram(&s->memory, OBJECT(dev), "ramfb-mmio",
                                RAMFB_MMIO_SIZE, errp)) {
        return;
    }
    memory_region_add_subregion(get_system_memory(), RAMFB_MMIO_BASE,
                                &s->memory);
    s->con = graphic_console_init(dev, 0, &ramfb_ops, s);
    fw_cfg_add_file_callback(fw_cfg, "etc/ramfb", NULL,
                             ramfb_config_write, s, &s->config,
                             sizeof(s->config), false);
}

static void ramfb_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ramfb_realize;
    dc->hotpluggable = false;
    dc->desc = "RAM framebuffer";
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}

static const TypeInfo ramfb_info = {
    .name = TYPE_RAMFB,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(RAMFBState),
    .class_init = ramfb_class_init,
};

static void ramfb_register_types(void)
{
    type_register_static(&ramfb_info);
}

type_init(ramfb_register_types)
