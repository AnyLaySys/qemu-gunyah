
#ifndef QEMU_ARM_GICV3_ITS_COMMON_H
#define QEMU_ARM_GICV3_ITS_COMMON_H

#include "hw/sysbus.h"
#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"

#define TYPE_ARM_GICV3_ITS "arm-gicv3-its"

#define ITS_CONTROL_SIZE 0x10000
#define ITS_TRANS_SIZE   0x10000
#define ITS_SIZE         (ITS_CONTROL_SIZE + ITS_TRANS_SIZE)

#define GITS_CTLR        0x0
#define GITS_IIDR        0x4
#define GITS_TYPER       0x8
#define GITS_CBASER      0x80
#define GITS_CWRITER     0x88
#define GITS_CREADR      0x90
#define GITS_BASER       0x100

#define GITS_TRANSLATER  0x0040

typedef struct {
    bool indirect;
    uint16_t entry_sz;
    uint32_t page_sz;
    uint32_t num_entries;
    uint64_t base_addr;
} TableDesc;

typedef struct {
    uint32_t num_entries;
    uint64_t base_addr;
} CmdQDesc;

struct GICv3ITSState {
    SysBusDevice parent_obj;

    MemoryRegion iomem_main;
    MemoryRegion iomem_its_cntrl;
    MemoryRegion iomem_its_translation;

    GICv3State *gicv3;

    int dev_fd; /* kvm device fd if backed by kvm vgic support */
    uint64_t gits_translater_gpa;
    bool translater_gpa_known;

    uint32_t ctlr;
    uint32_t iidr;
    uint64_t typer;
    uint64_t cbaser;
    uint64_t cwriter;
    uint64_t creadr;
    uint64_t baser[8];

    TableDesc  dt;
    TableDesc  ct;
    TableDesc  vpet;
    CmdQDesc   cq;

    Error *migration_blocker;
};

typedef struct GICv3ITSState GICv3ITSState;

void gicv3_its_init_mmio(GICv3ITSState *s, const MemoryRegionOps *ops,
                   const MemoryRegionOps *tops);

static inline void gicv3_add_its(GICv3State *s, DeviceState *its)
{
    g_ptr_array_add(s->itslist, its);
}

static inline void gicv3_foreach_its(GICv3State *s, GFunc func, void *opaque)
{
    g_ptr_array_foreach(s->itslist, func, opaque);
}

#define TYPE_ARM_GICV3_ITS_COMMON "arm-gicv3-its-common"
typedef struct GICv3ITSCommonClass GICv3ITSCommonClass;
DECLARE_OBJ_CHECKERS(GICv3ITSState, GICv3ITSCommonClass,
                     ARM_GICV3_ITS_COMMON, TYPE_ARM_GICV3_ITS_COMMON)

struct GICv3ITSCommonClass {
    SysBusDeviceClass parent_class;

    int (*send_msi)(GICv3ITSState *s, uint32_t data, uint16_t devid);
    void (*pre_save)(GICv3ITSState *s);
    void (*post_load)(GICv3ITSState *s);
};

const char *its_class_name(void);

#endif
