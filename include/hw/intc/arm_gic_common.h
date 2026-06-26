
#ifndef HW_ARM_GIC_COMMON_H
#define HW_ARM_GIC_COMMON_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define GIC_MAXIRQ 1020
#define GIC_INTERNAL 32
#define GIC_NR_SGIS 16
#define GIC_NCPU 8
#define GIC_NCPU_VCPU (GIC_NCPU * 2)

#define MAX_NR_GROUP_PRIO 128
#define GIC_NR_APRS (MAX_NR_GROUP_PRIO / 32)

#define GIC_MIN_BPR 0
#define GIC_MIN_ABPR (GIC_MIN_BPR + 1)

#define GIC_MAX_LR 64

#define GIC_VIRT_MAX_GROUP_PRIO_BITS 5
#define GIC_VIRT_MAX_NR_GROUP_PRIO (1 << GIC_VIRT_MAX_GROUP_PRIO_BITS)
#define GIC_VIRT_NR_APRS (GIC_VIRT_MAX_NR_GROUP_PRIO / 32)

#define GIC_VIRT_MIN_BPR 2
#define GIC_VIRT_MIN_ABPR (GIC_VIRT_MIN_BPR + 1)

typedef struct gic_irq_state {
    uint8_t enabled;
    uint8_t pending;
    uint8_t active;
    uint8_t level;
    bool model; /* 0 = N:N, 1 = 1:N */
    bool edge_trigger; /* true: edge-triggered, false: level-triggered  */
    uint8_t group;
} gic_irq_state;

struct GICState {
    SysBusDevice parent_obj;

    qemu_irq parent_irq[GIC_NCPU];
    qemu_irq parent_fiq[GIC_NCPU];
    qemu_irq parent_virq[GIC_NCPU];
    qemu_irq parent_vfiq[GIC_NCPU];
    qemu_irq parent_nmi[GIC_NCPU];
    qemu_irq parent_vnmi[GIC_NCPU];
    qemu_irq maintenance_irq[GIC_NCPU];

    uint32_t ctlr;
    uint32_t cpu_ctlr[GIC_NCPU_VCPU];

    gic_irq_state irq_state[GIC_MAXIRQ];
    uint8_t irq_target[GIC_MAXIRQ];
    uint8_t priority1[GIC_INTERNAL][GIC_NCPU];
    uint8_t priority2[GIC_MAXIRQ - GIC_INTERNAL];
    uint8_t sgi_pending[GIC_NR_SGIS][GIC_NCPU];

    uint16_t priority_mask[GIC_NCPU_VCPU];
    uint16_t running_priority[GIC_NCPU_VCPU];
    uint16_t current_pending[GIC_NCPU_VCPU];
    uint32_t n_prio_bits;

    uint8_t  bpr[GIC_NCPU_VCPU];
    uint8_t  abpr[GIC_NCPU_VCPU];

    uint32_t apr[GIC_NR_APRS][GIC_NCPU];
    uint32_t nsapr[GIC_NR_APRS][GIC_NCPU];

    uint32_t h_hcr[GIC_NCPU];
    uint32_t h_misr[GIC_NCPU];
    uint32_t h_lr[GIC_MAX_LR][GIC_NCPU];
    uint32_t h_apr[GIC_NCPU];

    uint32_t num_lrs;

    uint32_t num_cpu;

    MemoryRegion iomem; /* Distributor */
    struct GICState *backref[GIC_NCPU];
    MemoryRegion cpuiomem[GIC_NCPU + 1]; /* CPU interfaces */
    MemoryRegion vifaceiomem[GIC_NCPU + 1]; /* Virtual interfaces */
    MemoryRegion vcpuiomem; /* vCPU interface */

    uint32_t num_irq;
    uint32_t revision;
    bool security_extn;
    bool virt_extn;
    bool irq_reset_nonsecure; /* configure IRQs as group 1 (NS) on reset? */
    int dev_fd; /* kvm device fd if backed by kvm vgic support */
    Error *migration_blocker;
};
typedef struct GICState GICState;

#define TYPE_ARM_GIC_COMMON "arm_gic_common"
typedef struct ARMGICCommonClass ARMGICCommonClass;
DECLARE_OBJ_CHECKERS(GICState, ARMGICCommonClass,
                     ARM_GIC_COMMON, TYPE_ARM_GIC_COMMON)

struct ARMGICCommonClass {
    SysBusDeviceClass parent_class;

    void (*pre_save)(GICState *s);
    void (*post_load)(GICState *s);
};

void gic_init_irqs_and_mmio(GICState *s, qemu_irq_handler handler,
                            const MemoryRegionOps *ops,
                            const MemoryRegionOps *virt_ops);

#endif
