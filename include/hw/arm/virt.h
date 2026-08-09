
#ifndef QEMU_ARM_VIRT_H
#define QEMU_ARM_VIRT_H

#include "exec/hwaddr.h"
#include "qemu/notify.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/arm/bsa.h"

#include "hw/intc/arm_gicv3_common.h"
#include "qom/object.h"

#define NUM_VIRTIO_TRANSPORTS 32
#define NUM_SMMU_IRQS          4

#define PVTIME_SIZE_PER_CPU 64

enum {
    VIRT_FLASH,
    VIRT_MEM,
    VIRT_CPUPERIPHS,
    VIRT_GIC_DIST,
    VIRT_GIC_CPU,
    VIRT_GIC_V2M,
    VIRT_GIC_HYP,
    VIRT_GIC_VCPU,
    VIRT_GIC_ITS,
    VIRT_GIC_REDIST,
    VIRT_SMMU,
    VIRT_UART0,
    VIRT_MMIO,
    VIRT_RTC,
    VIRT_FW_CFG,
    VIRT_PCIE,
    VIRT_PCIE_MMIO,
    VIRT_PCIE_PIO,
    VIRT_PCIE_ECAM,
    VIRT_UART1,
    VIRT_SECURE_MEM,
    VIRT_PVTIME,
    VIRT_LOWMEMMAP_LAST,
};

enum {
    VIRT_HIGH_GIC_REDIST2 =  VIRT_LOWMEMMAP_LAST,
    VIRT_HIGH_PCIE_ECAM,
    VIRT_HIGH_PCIE_MMIO,
};

typedef enum VirtIOMMUType {
    VIRT_IOMMU_NONE,
    VIRT_IOMMU_SMMUV3,
    VIRT_IOMMU_VIRTIO,
} VirtIOMMUType;

typedef enum VirtMSIControllerType {
    VIRT_MSI_CTRL_NONE,
} VirtMSIControllerType;

typedef enum VirtGICType {
    VIRT_GIC_VERSION_MAX = 0,
    VIRT_GIC_VERSION_HOST = 1,
    VIRT_GIC_VERSION_2 = 2,
    VIRT_GIC_VERSION_3 = 3,
    VIRT_GIC_VERSION_4 = 4,
    VIRT_GIC_VERSION_NOSEL,
} VirtGICType;

#define VIRT_GIC_VERSION_2_MASK BIT(VIRT_GIC_VERSION_2)
#define VIRT_GIC_VERSION_3_MASK BIT(VIRT_GIC_VERSION_3)
#define VIRT_GIC_VERSION_4_MASK BIT(VIRT_GIC_VERSION_4)

struct VirtMachineClass {
    MachineClass parent;
    bool disallow_affinity_adjustment;
    bool no_pmu;
    bool claim_edge_triggered_timers;
    bool no_highmem_compact;
    bool no_highmem_ecam;
    bool kvm_no_adjvtime;
    bool no_kvm_steal_time;
    bool no_cpu_topology;
    bool no_ns_el2_virt_timer_irq;
    bool no_nested_smmu;
};

struct VirtMachineState {
    MachineState parent;
    Notifier machine_done;
    FWCfgState *fw_cfg;
    bool secure;
    bool highmem;
    bool highmem_compact;
    bool highmem_ecam;
    bool highmem_mmio;
    bool highmem_redists;
    bool virt;
    bool ras;
    bool dtb_randomness;
    bool second_ns_uart_present;
    OnOffAuto acpi;
    VirtGICType gic_version;
    VirtIOMMUType iommu;
    bool default_bus_bypass_iommu;
    VirtMSIControllerType msi_controller;
    uint16_t virtio_iommu_bdf;
    struct arm_boot_info bootinfo;
    MemMapEntry *memmap;
    char *pciehb_nodename;
    const int *irqmap;
    int fdt_size;
    uint32_t clock_phandle;
    uint32_t gic_phandle;
    uint32_t msi_phandle;
    uint32_t iommu_phandle;
    uint32_t restricted_dma_phandle;
    int psci_conduit;
    hwaddr highest_gpa;
    DeviceState *gic;
    PCIBus *bus;
    bool ns_el2_virt_timer_irq;
};

#define VIRT_ECAM_ID(high) (high ? VIRT_HIGH_PCIE_ECAM : VIRT_PCIE_ECAM)

#define TYPE_VIRT_MACHINE   MACHINE_TYPE_NAME("virt")
OBJECT_DECLARE_TYPE(VirtMachineState, VirtMachineClass, VIRT_MACHINE)

static uint32_t virt_redist_capacity(VirtMachineState *vms, int region)
{
    uint32_t redist_size;

    if (vms->gic_version == VIRT_GIC_VERSION_3) {
        redist_size = GICV3_REDIST_SIZE;
    } else {
        redist_size = GICV4_REDIST_SIZE;
    }
    return vms->memmap[region].size / redist_size;
}

static inline int virt_gicv3_redist_region_count(VirtMachineState *vms)
{
    uint32_t redist0_capacity = virt_redist_capacity(vms, VIRT_GIC_REDIST);

    assert(vms->gic_version != VIRT_GIC_VERSION_2);

    return (MACHINE(vms)->smp.cpus > redist0_capacity &&
            vms->highmem_redists) ? 2 : 1;
}

#endif /* QEMU_ARM_VIRT_H */
