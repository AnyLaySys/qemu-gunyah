#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/loader.h"
#include "hw/nvram/fw_cfg.h"
#include "ui/console.h"
#include "hw/display/ramfb.h"
#include "system/reset.h"
#include "exec/cpu-common.h"
#include "exec/address-spaces.h"
#include "qemu/memalign.h"

#define RAMFB_MAX_XRES 16000
#define RAMFB_MAX_YRES 12000
#define RAMFB_MMIO_BASE 0x0f000000
#define RAMFB_MMIO_SIZE 0x01000000

struct QEMU_PACKED RAMFBCfg {
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

typedef struct RAMFBCfg RAMFBCfg;

struct RAMFBState {
    DisplaySurface *ds;
    uint32_t width, height;
    struct RAMFBCfg cfg;
    MemoryRegion fb_mr;
    void *fb;
};

static void ramfb_unmap_display_surface(pixman_image_t *image, void *unused)
{
    void *data = pixman_image_get_data(image);
    uint32_t size = pixman_image_get_stride(image) *
        pixman_image_get_height(image);
    cpu_physical_memory_unmap(data, size, 0, 0);
}

static DisplaySurface *ramfb_create_display_surface(int width, int height,
                                                    pixman_format_code_t format,
                                                    hwaddr stride, hwaddr addr)
{
    DisplaySurface *surface;
    hwaddr size, mapsize, linesize;
    void *data;

    if (width < 16 || width > RAMFB_MAX_XRES ||
        height < 16 || height > RAMFB_MAX_YRES ||
        format == 0) {
        return NULL;
    }

    linesize = width * PIXMAN_FORMAT_BPP(format) / 8;
    if (stride == 0) {
        stride = linesize;
    }

    mapsize = size = stride * (height - 1) + linesize;
    data = cpu_physical_memory_map(addr, &mapsize, false);
    if (size != mapsize) {
        cpu_physical_memory_unmap(data, mapsize, 0, 0);
        return NULL;
    }

    surface = qemu_create_displaysurface_from(width, height,
                                              format, stride, data);
    pixman_image_set_destroy_function(surface->image,
                                      ramfb_unmap_display_surface, NULL);

    return surface;
}

static DisplaySurface *ramfb_create_mmio_display_surface(RAMFBState *s,
                                                         int width, int height,
                                                         pixman_format_code_t format,
                                                         hwaddr stride,
                                                         hwaddr addr)
{
    hwaddr size, offset, linesize;
    void *data;

    if (width < 16 || width > RAMFB_MAX_XRES ||
        height < 16 || height > RAMFB_MAX_YRES ||
        format == 0 ||
        addr < RAMFB_MMIO_BASE ||
        addr >= RAMFB_MMIO_BASE + RAMFB_MMIO_SIZE) {
        return NULL;
    }

    linesize = width * PIXMAN_FORMAT_BPP(format) / 8;
    if (stride == 0) {
        stride = linesize;
    }

    size = stride * (height - 1) + linesize;
    offset = addr - RAMFB_MMIO_BASE;
    if (offset > RAMFB_MMIO_SIZE || size > RAMFB_MMIO_SIZE - offset) {
        return NULL;
    }

    data = (uint8_t *)s->fb + offset;
    return qemu_create_displaysurface_from(width, height, format, stride, data);
}

static void ramfb_fw_cfg_write(void *dev, off_t offset, size_t len)
{
    RAMFBState *s = dev;
    DisplaySurface *surface;
    uint32_t fourcc, format, width, height;
    hwaddr stride, addr;

    width  = be32_to_cpu(s->cfg.width);
    height = be32_to_cpu(s->cfg.height);
    stride = be32_to_cpu(s->cfg.stride);
    fourcc = be32_to_cpu(s->cfg.fourcc);
    addr   = be64_to_cpu(s->cfg.addr);
    format = qemu_drm_format_to_pixman(fourcc);

    fprintf(stderr, "ramfb: cfg addr=0x%" PRIx64 " fourcc=0x%08x width=%u height=%u stride=0x%" PRIx64 "\n",
            (uint64_t)addr, fourcc, width, height, (uint64_t)stride);

    surface = ramfb_create_mmio_display_surface(s, width, height,
                                                format, stride, addr);
    if (!surface) {
        surface = ramfb_create_display_surface(width, height,
                                               format, stride, addr);
    }
    if (!surface) {
        fprintf(stderr, "ramfb: surface create failed\n");
        return;
    }

    s->width = width;
    s->height = height;
    qemu_free_displaysurface(s->ds);
    s->ds = surface;
    fprintf(stderr, "ramfb: surface ready\n");
}

void ramfb_display_update(QemuConsole *con, RAMFBState *s)
{
    if (!s->width || !s->height) {
        return;
    }

    if (s->ds) {
        dpy_gfx_replace_surface(con, s->ds);
        s->ds = NULL;
    }

    dpy_gfx_update_full(con);
}

static int ramfb_post_load(void *opaque, int version_id)
{
    ramfb_fw_cfg_write(opaque, 0, 0);
    return 0;
}

const VMStateDescription ramfb_vmstate = {
    .name = "ramfb",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = ramfb_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_BUFFER_UNSAFE(cfg, RAMFBState, 0, sizeof(RAMFBCfg)),
        VMSTATE_END_OF_LIST()
    }
};

RAMFBState *ramfb_setup(Error **errp)
{
    FWCfgState *fw_cfg = fw_cfg_find();
    RAMFBState *s;

    if (!fw_cfg || !fw_cfg->dma_enabled) {
        error_setg(errp, "ramfb device requires fw_cfg with DMA");
        return NULL;
    }

    s = g_new0(RAMFBState, 1);
    s->fb = qemu_memalign(qemu_real_host_page_size(), RAMFB_MMIO_SIZE);
    memset(s->fb, 0, RAMFB_MMIO_SIZE);
    memory_region_init_ram_ptr(&s->fb_mr, NULL, "ramfb-mmio",
                               RAMFB_MMIO_SIZE, s->fb);
    memory_region_add_subregion(get_system_memory(), RAMFB_MMIO_BASE,
                                &s->fb_mr);
    fprintf(stderr, "ramfb: mmio window gpa=0x%x size=0x%x hva=%p\n",
            RAMFB_MMIO_BASE, RAMFB_MMIO_SIZE, s->fb);
    fw_cfg_add_file_callback(fw_cfg, "etc/ramfb",
                             NULL, ramfb_fw_cfg_write, s,
                             &s->cfg, sizeof(s->cfg), false);
    return s;
}
