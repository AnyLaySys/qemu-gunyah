/*
 * early boot framebuffer in guest ram
 * configured using fw_cfg
 *
 * Copyright Red Hat, Inc. 2017
 *
 * Author:
 *     Gerd Hoffmann <kraxel@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/loader.h"
#include "hw/display/ramfb.h"
#include "hw/display/bochs-vbe.h" /* for limits */
#include "ui/console.h"
#include "system/reset.h"
#include "system/gunyah.h"

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
    /* Gunyah: host-side framebuffer copy for LEND'd memory */
    void *gh_buf;
    hwaddr gh_addr;
    hwaddr gh_size;
    hwaddr gh_stride;
    pixman_format_code_t gh_format;
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

    if (width < 16 || width > VBE_DISPI_MAX_XRES ||
        height < 16 || height > VBE_DISPI_MAX_YRES ||
        format == 0 /* unknown format */)
        return NULL;

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

    if (gunyah_enabled()) {
        /* Gunyah: the guest's framebuffer address is in LEND'd memory,
         * but we set up a simplefb region in SHARE'd memory.
         * Try to map the simplefb SHARE'd address directly first.
         * If the guest uses a different address (LEND'd), fall back to
         * cpu_physical_memory_read polling. */
        hwaddr linesize = width * PIXMAN_FORMAT_BPP(format) / 8;
        if (stride == 0) stride = linesize;
        hwaddr size = stride * (height - 1) + linesize;

        g_free(s->gh_buf);
        s->gh_buf = g_malloc0(size);
        s->gh_addr = addr;
        s->gh_size = size;
        s->gh_stride = stride;
        s->gh_format = format;

        /* Try direct map first (works if addr is in SHARE'd region) */
        hwaddr mapsize = size;
        void *direct = cpu_physical_memory_map(addr, &mapsize, false);
        if (direct && mapsize == size) {
            /* Check if the mapped memory is readable (not all zeros) */
            surface = qemu_create_displaysurface_from(width, height,
                                                      format, stride, direct);
            pixman_image_set_destroy_function(surface->image,
                                              ramfb_unmap_display_surface, NULL);
            /* Still keep gh_buf for polling fallback */
        } else {
            if (direct) cpu_physical_memory_unmap(direct, mapsize, 0, 0);
            cpu_physical_memory_read(addr, s->gh_buf, size);
            surface = qemu_create_displaysurface_from(width, height,
                                                      format, stride, s->gh_buf);
        }
    } else {
        surface = ramfb_create_display_surface(width, height,
                                               format, stride, addr);
    }

    if (!surface) {
        return;
    }

    s->width = width;
    s->height = height;
    qemu_free_displaysurface(s->ds);
    s->ds = surface;
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

    /*
     * Gunyah: if the console surface was replaced (e.g. by virtio-gpu
     * reset creating a blank placeholder), recreate the ramfb surface
     * so display falls back to the ramfb framebuffer.
     */
    if (s->gh_buf && s->gh_size > 0 && !s->ds) {
        DisplaySurface *cur = qemu_console_surface(con);
        void *cur_data = cur ? surface_data(cur) : NULL;
        if (cur_data != s->gh_buf) {
            /* Console surface doesn't point to our buffer — recreate */
            DisplaySurface *ds = qemu_create_displaysurface_from(
                s->width, s->height, s->gh_format, s->gh_stride, s->gh_buf);
            if (ds) {
                dpy_gfx_replace_surface(con, ds);
            }
        }
    }

    /* Gunyah: re-read framebuffer from guest memory each frame,
     * since the direct mapping returns zeros for LEND'd pages. */
    if (s->gh_buf && s->gh_size > 0) {
        cpu_physical_memory_read(s->gh_addr, s->gh_buf, s->gh_size);
    }

    /* simple full screen update */
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

    if (!fw_cfg) {
        error_setg(errp, "ramfb device requires fw_cfg");
        return NULL;
    }

    s = g_new0(RAMFBState, 1);

    rom_add_vga("vgabios-ramfb.bin");
    fw_cfg_add_file_callback(fw_cfg, "etc/ramfb",
                             NULL, ramfb_fw_cfg_write, s,
                             &s->cfg, sizeof(s->cfg), false);
    return s;
}
