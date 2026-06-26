
#ifndef HW_ARM_GICV3_COMMON_H
#define HW_ARM_GICV3_COMMON_H

#include "hw/sysbus.h"
#include "hw/intc/arm_gic_common.h"
#include "qom/object.h"

#define GICV3_MAXIRQ 1020
#define GICV3_MAXSPI (GICV3_MAXIRQ - GIC_INTERNAL)

#define GICV3_LPI_INTID_START 8192

#define GICV3_REDIST_SIZE 0x20000
#define GICV4_REDIST_SIZE 0x40000

#define GICV3_TARGETLIST_BITS 16

#define GICV3_LR_MAX 16

#define GIC_DECLARE_BITMAP(name) DECLARE_BITMAP32(name, GICV3_MAXIRQ)
#define GICV3_BMP_SIZE BITS_TO_U32S(GICV3_MAXIRQ)

static inline void gic_bmp_replace_bit(int nr, uint32_t *addr, int val)
{
    uint32_t mask = BIT32_MASK(nr);
    uint32_t *p = addr + BIT32_WORD(nr);

    *p &= ~mask;
    *p |= (val & 1U) << (nr % 32);
}

static inline uint32_t *gic_bmp_ptr32(uint32_t *addr, int nr)
{
    return addr + BIT32_WORD(nr);
}

typedef struct GICv3State GICv3State;
typedef struct GICv3CPUState GICv3CPUState;

#define GICV3_G0 0
#define GICV3_G1 1
#define GICV3_G1NS 2

#define GICV3_S 0
#define GICV3_NS 1

typedef struct {
    int irq;
    uint8_t prio;
    int grp;
    bool nmi;
} PendingIrq;

struct GICv3CPUState {
    GICv3State *gic;
    CPUState *cpu;
    qemu_irq parent_irq;
    qemu_irq parent_fiq;
    qemu_irq parent_virq;
    qemu_irq parent_vfiq;
    qemu_irq parent_nmi;
    qemu_irq parent_vnmi;

    uint32_t level;                  /* Current IRQ level */
    uint32_t gicr_ctlr;
    uint64_t gicr_typer;
    uint32_t gicr_statusr[2];
    uint32_t gicr_waker;
    uint64_t gicr_propbaser;
    uint64_t gicr_pendbaser;
    uint32_t gicr_igroupr0;
    uint32_t gicr_ienabler0;
    uint32_t gicr_ipendr0;
    uint32_t gicr_iactiver0;
    uint32_t gicr_inmir0;
    uint32_t edge_trigger; /* ICFGR0 and ICFGR1 even bits */
    uint32_t gicr_igrpmodr0;
    uint32_t gicr_nsacr;
    uint8_t gicr_ipriorityr[GIC_INTERNAL];
    uint64_t gicr_vpropbaser;
    uint64_t gicr_vpendbaser;

    uint64_t icc_sre_el1;
    uint64_t icc_ctlr_el1[2];
    uint64_t icc_pmr_el1;
    uint64_t icc_bpr[3];
    uint64_t icc_apr[3][4];
    uint64_t icc_igrpen[3];
    uint64_t icc_ctlr_el3;

    uint64_t ich_apr[3][4]; /* ich_apr[GICV3_G1][x] never used */
    uint64_t ich_hcr_el2;
    uint64_t ich_lr_el2[GICV3_LR_MAX];
    uint64_t ich_vmcr_el2;

    int num_list_regs;
    int vpribits; /* number of virtual priority bits */
    int vprebits; /* number of virtual preemption bits */
    int pribits; /* number of physical priority bits */
    int prebits; /* number of physical preemption bits */

    PendingIrq hppi;

    PendingIrq hpplpi;

    PendingIrq hppvlpi;

    bool seenbetter;

    bool nmi_support;
};

typedef struct GICv3RedistRegion {
    GICv3State *gic;
    MemoryRegion iomem;
    uint32_t cpuidx; /* index of first CPU this region covers */
} GICv3RedistRegion;

struct GICv3State {
    SysBusDevice parent_obj;

    MemoryRegion iomem_dist; /* Distributor */
    GICv3RedistRegion *redist_regions; /* Redistributor Regions */
    uint32_t *redist_region_count; /* redistributor count within each region */
    uint32_t nb_redist_regions; /* number of redist regions */

    uint32_t num_cpu;
    uint32_t num_irq;
    uint32_t revision;
    bool lpi_enable;
    bool nmi_support;
    bool security_extn;
    bool force_8bit_prio;
    bool irq_reset_nonsecure;
    bool gicd_no_migration_shift_bug;

    int dev_fd; /* kvm device fd if backed by kvm vgic support */
    Error *migration_blocker;

    MemoryRegion *dma;
    AddressSpace dma_as;


    uint32_t gicd_ctlr;
    uint32_t gicd_statusr[2];
    GIC_DECLARE_BITMAP(group);        /* GICD_IGROUPR */
    GIC_DECLARE_BITMAP(grpmod);       /* GICD_IGRPMODR */
    GIC_DECLARE_BITMAP(enabled);      /* GICD_ISENABLER */
    GIC_DECLARE_BITMAP(pending);      /* GICD_ISPENDR */
    GIC_DECLARE_BITMAP(active);       /* GICD_ISACTIVER */
    GIC_DECLARE_BITMAP(level);        /* Current level */
    GIC_DECLARE_BITMAP(edge_trigger); /* GICD_ICFGR even bits */
    GIC_DECLARE_BITMAP(nmi);          /* GICD_INMIR */
    uint8_t gicd_ipriority[GICV3_MAXIRQ];
    uint64_t gicd_irouter[GICV3_MAXIRQ];
    GICv3CPUState *gicd_irouter_target[GICV3_MAXIRQ];
    uint32_t gicd_nsacr[DIV_ROUND_UP(GICV3_MAXIRQ, 16)];

    GICv3CPUState *cpu;
    GPtrArray *itslist;
};

#define GICV3_BITMAP_ACCESSORS(BMP)                                     \
    static inline void gicv3_gicd_##BMP##_set(GICv3State *s, int irq)   \
    {                                                                   \
        set_bit32(irq, s->BMP);                                         \
    }                                                                   \
    static inline int gicv3_gicd_##BMP##_test(GICv3State *s, int irq)   \
    {                                                                   \
        return test_bit32(irq, s->BMP);                                 \
    }                                                                   \
    static inline void gicv3_gicd_##BMP##_clear(GICv3State *s, int irq) \
    {                                                                   \
        clear_bit32(irq, s->BMP);                                       \
    }                                                                   \
    static inline void gicv3_gicd_##BMP##_replace(GICv3State *s,        \
                                                  int irq, int value)   \
    {                                                                   \
        gic_bmp_replace_bit(irq, s->BMP, value);                        \
    }

GICV3_BITMAP_ACCESSORS(group)
GICV3_BITMAP_ACCESSORS(grpmod)
GICV3_BITMAP_ACCESSORS(enabled)
GICV3_BITMAP_ACCESSORS(pending)
GICV3_BITMAP_ACCESSORS(active)
GICV3_BITMAP_ACCESSORS(level)
GICV3_BITMAP_ACCESSORS(edge_trigger)
GICV3_BITMAP_ACCESSORS(nmi)

#define TYPE_ARM_GICV3_COMMON "arm-gicv3-common"
typedef struct ARMGICv3CommonClass ARMGICv3CommonClass;
DECLARE_OBJ_CHECKERS(GICv3State, ARMGICv3CommonClass,
                     ARM_GICV3_COMMON, TYPE_ARM_GICV3_COMMON)

struct ARMGICv3CommonClass {
    SysBusDeviceClass parent_class;

    void (*pre_save)(GICv3State *s);
    void (*post_load)(GICv3State *s);
};

void gicv3_init_irqs_and_mmio(GICv3State *s, qemu_irq_handler handler,
                              const MemoryRegionOps *ops);

const char *gicv3_class_name(void);

#endif
