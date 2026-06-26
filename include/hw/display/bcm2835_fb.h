
#ifndef BCM2835_FB_H
#define BCM2835_FB_H

#include "hw/sysbus.h"
#include "ui/console.h"
#include "qom/object.h"

#define UPPER_RAM_BASE 0x40000000

#define TYPE_BCM2835_FB "bcm2835-fb"
OBJECT_DECLARE_SIMPLE_TYPE(BCM2835FBState, BCM2835_FB)

typedef struct {
    uint32_t xres, yres;
    uint32_t xres_virtual, yres_virtual;
    uint32_t xoffset, yoffset;
    uint32_t bpp;
    uint32_t base;
    uint32_t pixo;
    uint32_t alpha;
} BCM2835FBConfig;

struct BCM2835FBState {
    SysBusDevice busdev;

    uint32_t vcram_base, vcram_size;
    MemoryRegion *dma_mr;
    AddressSpace dma_as;
    MemoryRegion iomem;
    MemoryRegionSection fbsection;
    QemuConsole *con;
    qemu_irq mbox_irq;

    bool lock, invalidate, pending;

    BCM2835FBConfig config;
    BCM2835FBConfig initial_config;
};

void bcm2835_fb_reconfigure(BCM2835FBState *s, BCM2835FBConfig *newconfig);

static inline uint32_t bcm2835_fb_get_pitch(BCM2835FBConfig *config)
{
    uint32_t xres = MAX(config->xres, config->xres_virtual);
    return xres * (config->bpp >> 3);
}

static inline uint32_t bcm2835_fb_get_size(BCM2835FBConfig *config)
{
    uint32_t yres = MAX(config->yres, config->yres_virtual);
    return yres * bcm2835_fb_get_pitch(config);
}

void bcm2835_fb_validate_config(BCM2835FBConfig *config);

#endif
