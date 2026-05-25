/*
 * Simple Framebuffer device for Gunyah VMs
 *
 * Provides a basic framebuffer by periodically reading guest RAM
 * at a fixed address and displaying via the QEMU console.
 * Compatible with the Linux/UEFI "simple-framebuffer" DT binding.
 *
 * The framebuffer address is injected into the device tree by
 * virt_modify_dtb() when -device simplefb is specified.
 *
 * Copyright 2026, QEMU-NDK contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "exec/address-spaces.h"

#define TYPE_SIMPLEFB "simplefb"
OBJECT_DECLARE_SIMPLE_TYPE(SimpleFBState, SIMPLEFB)

#define SIMPLEFB_DEFAULT_WIDTH  1280
#define SIMPLEFB_DEFAULT_HEIGHT 720
#define SIMPLEFB_BPP            4
#define SIMPLEFB_REFRESH_MS     33  /* ~30 fps */

struct SimpleFBState {
    SysBusDevice parent_obj;

    QemuConsole *con;
    QEMUTimer *timer;

    uint32_t width;
    uint32_t height;
    uint64_t fb_addr;  /* GPA of framebuffer in guest RAM */
    uint64_t fb_size;

    void *buf;         /* host-side copy of framebuffer */
};

static void simplefb_update(void *opaque)
{
    SimpleFBState *s = SIMPLEFB(opaque);
    DisplaySurface *surface;

    if (!s->fb_addr || !s->buf) {
        timer_mod(s->timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) +
                  SIMPLEFB_REFRESH_MS);
        return;
    }

    /* Read framebuffer from guest RAM */
    cpu_physical_memory_read(s->fb_addr, s->buf, s->fb_size);

    surface = qemu_create_displaysurface_from(
        s->width, s->height,
        PIXMAN_x8r8g8b8,        /* DRM_FORMAT_XRGB8888 */
        s->width * SIMPLEFB_BPP,
        s->buf);

    if (surface) {
        dpy_gfx_replace_surface(s->con, surface);
        dpy_gfx_update_full(s->con);
    }

    timer_mod(s->timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) +
              SIMPLEFB_REFRESH_MS);
}

static void simplefb_realize(DeviceState *dev, Error **errp)
{
    SimpleFBState *s = SIMPLEFB(dev);
    uint32_t stride;

    if (!s->width) {
        s->width = SIMPLEFB_DEFAULT_WIDTH;
    }
    if (!s->height) {
        s->height = SIMPLEFB_DEFAULT_HEIGHT;
    }

    stride = s->width * SIMPLEFB_BPP;
    s->fb_size = (uint64_t)stride * s->height;

    if (!s->fb_addr) {
        /* Will be set later by virt_modify_dtb */
        return;
    }

    s->buf = g_malloc0(s->fb_size);

    s->con = graphic_console_init(dev, 0, NULL, NULL);
    s->timer = timer_new_ms(QEMU_CLOCK_REALTIME, simplefb_update, s);

    timer_mod(s->timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) +
              SIMPLEFB_REFRESH_MS);

    fprintf(stderr, "simplefb: %ux%u at GPA 0x%"PRIx64" (%"PRIu64" bytes)\n",
            s->width, s->height, s->fb_addr, s->fb_size);
}

/* Called from virt.c after DTB is built and fb_addr is known */
void simplefb_start(uint64_t fb_addr, uint32_t width, uint32_t height)
{
    DeviceState *dev;
    SimpleFBState *s;
    uint32_t stride = width * SIMPLEFB_BPP;

    dev = qdev_new(TYPE_SIMPLEFB);
    s = SIMPLEFB(dev);
    s->width = width;
    s->height = height;
    s->fb_addr = fb_addr;
    s->fb_size = (uint64_t)stride * height;
    s->buf = g_malloc0(s->fb_size);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    s->con = graphic_console_init(dev, 0, NULL, NULL);
    s->timer = timer_new_ms(QEMU_CLOCK_REALTIME, simplefb_update, s);

    timer_mod(s->timer, qemu_clock_get_ms(QEMU_CLOCK_REALTIME) +
              SIMPLEFB_REFRESH_MS);

    fprintf(stderr, "simplefb: started %ux%u at GPA 0x%"PRIx64"\n",
            width, height, fb_addr);
}

static const Property simplefb_properties[] = {
    DEFINE_PROP_UINT32("width", SimpleFBState, width, SIMPLEFB_DEFAULT_WIDTH),
    DEFINE_PROP_UINT32("height", SimpleFBState, height, SIMPLEFB_DEFAULT_HEIGHT),
    DEFINE_PROP_UINT64("fb_addr", SimpleFBState, fb_addr, 0),
};

static void simplefb_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = simplefb_realize;
    device_class_set_props(dc, simplefb_properties);
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}

static const TypeInfo simplefb_info = {
    .name          = TYPE_SIMPLEFB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SimpleFBState),
    .class_init    = simplefb_class_init,
};

static void simplefb_register_types(void)
{
    type_register_static(&simplefb_info);
}

type_init(simplefb_register_types)
