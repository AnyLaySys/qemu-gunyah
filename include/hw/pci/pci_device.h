#ifndef QEMU_PCI_DEVICE_H
#define QEMU_PCI_DEVICE_H

#include "hw/pci/pci.h"
#include "hw/pci/pcie.h"
#include "hw/pci/pcie_doe.h"

#define TYPE_PCI_DEVICE "pci-device"
typedef struct PCIDeviceClass PCIDeviceClass;
DECLARE_OBJ_CHECKERS(PCIDevice, PCIDeviceClass,
                     PCI_DEVICE, TYPE_PCI_DEVICE)

#define INTERFACE_PCIE_DEVICE "pci-express-device"

#define INTERFACE_CONVENTIONAL_PCI_DEVICE "conventional-pci-device"

struct PCIDeviceClass {
    DeviceClass parent_class;

    void (*realize)(PCIDevice *dev, Error **errp);
    PCIUnregisterFunc *exit;
    PCIConfigReadFunc *config_read;
    PCIConfigWriteFunc *config_write;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
    uint16_t class_id;
    uint16_t subsystem_vendor_id;       /* only for header type = 0 */
    uint16_t subsystem_id;              /* only for header type = 0 */

    const char *romfile;                /* rom bar */
};

enum PCIReqIDType {
    PCI_REQ_ID_INVALID = 0,
    PCI_REQ_ID_BDF,
    PCI_REQ_ID_SECONDARY_BUS,
    PCI_REQ_ID_MAX,
};
typedef enum PCIReqIDType PCIReqIDType;

struct PCIReqIDCache {
    PCIDevice *dev;
    PCIReqIDType type;
};
typedef struct PCIReqIDCache PCIReqIDCache;

struct PCIDevice {
    DeviceState qdev;
    bool partially_hotplugged;
    bool enabled;

    uint8_t *config;

    uint8_t *cmask;

    uint8_t *wmask;

    uint8_t *w1cmask;

    uint8_t *used;

    int32_t devfn;
    PCIReqIDCache requester_id_cache;
    char name[64];
    PCIIORegion io_regions[PCI_NUM_REGIONS];
    AddressSpace bus_master_as;
    MemoryRegion bus_master_container_region;
    MemoryRegion bus_master_enable_region;

    PCIConfigReadFunc *config_read;
    PCIConfigWriteFunc *config_write;

    MemoryRegion *vga_regions[QEMU_PCI_VGA_NUM_REGIONS];
    bool has_vga;

    uint8_t irq_state;

    uint32_t cap_present;

    uint8_t pm_cap;

    uint8_t msix_cap;

    int msix_entries_nr;

    uint8_t *msix_table;
    uint8_t *msix_pba;

    void *irq_opaque;

    MSITriggerFunc *msi_trigger;
    MSIPrepareMessageFunc *msi_prepare_message;
    MSIxPrepareMessageFunc *msix_prepare_message;

    MemoryRegion msix_exclusive_bar;
    MemoryRegion msix_table_mmio;
    MemoryRegion msix_pba_mmio;
    unsigned *msix_entry_used;
    bool msix_function_masked;
    int32_t version_id;

    uint8_t msi_cap;

    PCIExpressDevice exp;

    SHPCDevice *shpc;

    char *romfile;
    uint32_t romsize;
    bool has_rom;
    MemoryRegion rom;
    int32_t rom_bar;

    PCIINTxRoutingNotifier intx_routing_notifier;

    MSIVectorUseNotifier msix_vector_use_notifier;
    MSIVectorReleaseNotifier msix_vector_release_notifier;
    MSIVectorPollNotifier msix_vector_poll_notifier;

    uint16_t spdm_port;

    DOECap doe_spdm;

    char *failover_pair_id;
    uint32_t acpi_index;

    uint32_t max_bounce_buffer_size;
};

static inline int pci_intx(PCIDevice *pci_dev)
{
    return pci_get_byte(pci_dev->config + PCI_INTERRUPT_PIN) - 1;
}

static inline int pci_is_express(const PCIDevice *d)
{
    return d->cap_present & QEMU_PCI_CAP_EXPRESS;
}

static inline int pci_is_express_downstream_port(const PCIDevice *d)
{
    uint8_t type;

    if (!pci_is_express(d) || !d->exp.exp_cap) {
        return 0;
    }

    type = pcie_cap_get_type(d);

    return type == PCI_EXP_TYPE_DOWNSTREAM || type == PCI_EXP_TYPE_ROOT_PORT;
}

static inline int pci_is_vf(const PCIDevice *d)
{
    return d->exp.sriov_vf.pf != NULL;
}

static inline uint32_t pci_config_size(const PCIDevice *d)
{
    return pci_is_express(d) ? PCIE_CONFIG_SPACE_SIZE : PCI_CONFIG_SPACE_SIZE;
}

static inline uint16_t pci_get_bdf(PCIDevice *dev)
{
    return PCI_BUILD_BDF(pci_bus_num(pci_get_bus(dev)), dev->devfn);
}

uint16_t pci_requester_id(PCIDevice *dev);

static inline AddressSpace *pci_get_address_space(PCIDevice *dev)
{
    return &dev->bus_master_as;
}

static inline MemTxResult pci_dma_rw(PCIDevice *dev, dma_addr_t addr,
                                     void *buf, dma_addr_t len,
                                     DMADirection dir, MemTxAttrs attrs)
{
    return dma_memory_rw(pci_get_address_space(dev), addr, buf, len,
                         dir, attrs);
}

static inline MemTxResult pci_dma_read(PCIDevice *dev, dma_addr_t addr,
                                       void *buf, dma_addr_t len)
{
    return pci_dma_rw(dev, addr, buf, len,
                      DMA_DIRECTION_TO_DEVICE, MEMTXATTRS_UNSPECIFIED);
}

static inline MemTxResult pci_dma_write(PCIDevice *dev, dma_addr_t addr,
                                        const void *buf, dma_addr_t len)
{
    return pci_dma_rw(dev, addr, (void *) buf, len,
                      DMA_DIRECTION_FROM_DEVICE, MEMTXATTRS_UNSPECIFIED);
}

#define PCI_DMA_DEFINE_LDST(_l, _s, _bits) \
    static inline MemTxResult ld##_l##_pci_dma(PCIDevice *dev, \
                                               dma_addr_t addr, \
                                               uint##_bits##_t *val, \
                                               MemTxAttrs attrs) \
    { \
        return ld##_l##_dma(pci_get_address_space(dev), addr, val, attrs); \
    } \
    static inline MemTxResult st##_s##_pci_dma(PCIDevice *dev, \
                                               dma_addr_t addr, \
                                               uint##_bits##_t val, \
                                               MemTxAttrs attrs) \
    { \
        return st##_s##_dma(pci_get_address_space(dev), addr, val, attrs); \
    }

PCI_DMA_DEFINE_LDST(ub, b, 8);
PCI_DMA_DEFINE_LDST(uw_le, w_le, 16)
PCI_DMA_DEFINE_LDST(l_le, l_le, 32);
PCI_DMA_DEFINE_LDST(q_le, q_le, 64);
PCI_DMA_DEFINE_LDST(uw_be, w_be, 16)
PCI_DMA_DEFINE_LDST(l_be, l_be, 32);
PCI_DMA_DEFINE_LDST(q_be, q_be, 64);

#undef PCI_DMA_DEFINE_LDST

static inline void *pci_dma_map(PCIDevice *dev, dma_addr_t addr,
                                dma_addr_t *plen, DMADirection dir)
{
    return dma_memory_map(pci_get_address_space(dev), addr, plen, dir,
                          MEMTXATTRS_UNSPECIFIED);
}

static inline void pci_dma_unmap(PCIDevice *dev, void *buffer, dma_addr_t len,
                                 DMADirection dir, dma_addr_t access_len)
{
    dma_memory_unmap(pci_get_address_space(dev), buffer, len, dir, access_len);
}

static inline void pci_dma_sglist_init(QEMUSGList *qsg, PCIDevice *dev,
                                       int alloc_hint)
{
    qemu_sglist_init(qsg, DEVICE(dev), alloc_hint, pci_get_address_space(dev));
}

extern const VMStateDescription vmstate_pci_device;

#define VMSTATE_PCI_DEVICE(_field, _state) {                         \
    .name       = (stringify(_field)),                               \
    .size       = sizeof(PCIDevice),                                 \
    .vmsd       = &vmstate_pci_device,                               \
    .flags      = VMS_STRUCT,                                        \
    .offset     = vmstate_offset_value(_state, _field, PCIDevice),   \
}

#define VMSTATE_PCI_DEVICE_POINTER(_field, _state) {                 \
    .name       = (stringify(_field)),                               \
    .size       = sizeof(PCIDevice),                                 \
    .vmsd       = &vmstate_pci_device,                               \
    .flags      = VMS_STRUCT | VMS_POINTER,                          \
    .offset     = vmstate_offset_pointer(_state, _field, PCIDevice), \
}

#endif
