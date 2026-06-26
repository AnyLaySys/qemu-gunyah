
#ifndef QEMU_VIRTIO_PCI_H
#define QEMU_VIRTIO_PCI_H

#include "hw/pci/msi.h"
#include "hw/virtio/virtio-bus.h"
#include "qom/object.h"



typedef struct VirtioBusState VirtioPCIBusState;
typedef struct VirtioBusClass VirtioPCIBusClass;

#define TYPE_VIRTIO_PCI_BUS "virtio-pci-bus"
DECLARE_OBJ_CHECKERS(VirtioPCIBusState, VirtioPCIBusClass,
                     VIRTIO_PCI_BUS, TYPE_VIRTIO_PCI_BUS)

enum {
    VIRTIO_PCI_FLAG_BUS_MASTER_BUG_MIGRATION_BIT,
    VIRTIO_PCI_FLAG_USE_IOEVENTFD_BIT,
    VIRTIO_PCI_FLAG_MIGRATE_EXTRA_BIT,
    VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY_BIT,
    VIRTIO_PCI_FLAG_DISABLE_PCIE_BIT,
    VIRTIO_PCI_FLAG_PAGE_PER_VQ_BIT,
    VIRTIO_PCI_FLAG_ATS_BIT,
    VIRTIO_PCI_FLAG_INIT_DEVERR_BIT,
    VIRTIO_PCI_FLAG_INIT_LNKCTL_BIT,
    VIRTIO_PCI_FLAG_INIT_PM_BIT,
    VIRTIO_PCI_FLAG_INIT_FLR_BIT,
    VIRTIO_PCI_FLAG_AER_BIT,
    VIRTIO_PCI_FLAG_ATS_PAGE_ALIGNED_BIT,
    VIRTIO_PCI_FLAG_PM_NO_SOFT_RESET_BIT,
};

#define VIRTIO_PCI_FLAG_BUS_MASTER_BUG_MIGRATION \
    (1 << VIRTIO_PCI_FLAG_BUS_MASTER_BUG_MIGRATION_BIT)

#define VIRTIO_PCI_FLAG_USE_IOEVENTFD   (1 << VIRTIO_PCI_FLAG_USE_IOEVENTFD_BIT)

#define VIRTIO_PCI_FLAG_DISABLE_PCIE (1 << VIRTIO_PCI_FLAG_DISABLE_PCIE_BIT)

#define VIRTIO_PCI_FLAG_MIGRATE_EXTRA (1 << VIRTIO_PCI_FLAG_MIGRATE_EXTRA_BIT)

#define VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY \
    (1 << VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY_BIT)

#define VIRTIO_PCI_FLAG_PAGE_PER_VQ \
    (1 << VIRTIO_PCI_FLAG_PAGE_PER_VQ_BIT)

#define VIRTIO_PCI_FLAG_ATS (1 << VIRTIO_PCI_FLAG_ATS_BIT)

#define VIRTIO_PCI_FLAG_INIT_DEVERR (1 << VIRTIO_PCI_FLAG_INIT_DEVERR_BIT)

#define VIRTIO_PCI_FLAG_INIT_LNKCTL (1 << VIRTIO_PCI_FLAG_INIT_LNKCTL_BIT)

#define VIRTIO_PCI_FLAG_INIT_PM (1 << VIRTIO_PCI_FLAG_INIT_PM_BIT)

#define VIRTIO_PCI_FLAG_PM_NO_SOFT_RESET \
  (1 << VIRTIO_PCI_FLAG_PM_NO_SOFT_RESET_BIT)

#define VIRTIO_PCI_FLAG_INIT_FLR (1 << VIRTIO_PCI_FLAG_INIT_FLR_BIT)

#define VIRTIO_PCI_FLAG_AER (1 << VIRTIO_PCI_FLAG_AER_BIT)

#define VIRTIO_PCI_FLAG_ATS_PAGE_ALIGNED \
  (1 << VIRTIO_PCI_FLAG_ATS_PAGE_ALIGNED_BIT)

typedef struct {
    MSIMessage msg;
    int virq;
    unsigned int users;
} VirtIOIRQFD;

#define TYPE_VIRTIO_PCI "virtio-pci"
OBJECT_DECLARE_TYPE(VirtIOPCIProxy, VirtioPCIClass, VIRTIO_PCI)

struct VirtioPCIClass {
    PCIDeviceClass parent_class;
    DeviceRealize parent_dc_realize;
    void (*realize)(VirtIOPCIProxy *vpci_dev, Error **errp);
};

typedef struct VirtIOPCIRegion {
    MemoryRegion mr;
    uint32_t offset;
    uint32_t size;
    uint32_t type;
} VirtIOPCIRegion;

typedef struct VirtIOPCIQueue {
  uint16_t num;
  bool enabled;
  bool reset;
  uint32_t desc[2];
  uint32_t avail[2];
  uint32_t used[2];
} VirtIOPCIQueue;

struct VirtIOPCIProxy {
    PCIDevice pci_dev;
    MemoryRegion bar;
    union {
        struct {
            VirtIOPCIRegion common;
            VirtIOPCIRegion isr;
            VirtIOPCIRegion device;
            VirtIOPCIRegion notify;
            VirtIOPCIRegion notify_pio;
        };
        VirtIOPCIRegion regs[5];
    };
    MemoryRegion modern_bar;
    MemoryRegion io_bar;
    AddressSpace modern_cfg_mem_as;
    AddressSpace modern_cfg_io_as;
    uint32_t legacy_io_bar_idx;
    uint32_t msix_bar_idx;
    uint32_t modern_io_bar_idx;
    uint32_t modern_mem_bar_idx;
    int config_cap;
    uint32_t flags;
    bool disable_modern;
    bool ignore_backend_features;
    OnOffAuto disable_legacy;
    uint16_t trans_devid;
    uint32_t class_code;
    uint32_t nvectors;
    uint32_t dfselect;
    uint32_t gfselect;
    uint32_t guest_features[2];
    VirtIOPCIQueue vqs[VIRTIO_QUEUE_MAX];

    VirtIOIRQFD *vector_irqfd;
    int nvqs_with_notifiers;
    VirtioBusState bus;
};

static inline bool virtio_pci_modern(VirtIOPCIProxy *proxy)
{
    return !proxy->disable_modern;
}

static inline bool virtio_pci_legacy(VirtIOPCIProxy *proxy)
{
    return proxy->disable_legacy == ON_OFF_AUTO_OFF;
}

static inline void virtio_pci_force_virtio_1(VirtIOPCIProxy *proxy)
{
    proxy->disable_modern = false;
    proxy->disable_legacy = ON_OFF_AUTO_ON;
}

static inline void virtio_pci_disable_modern(VirtIOPCIProxy *proxy)
{
    proxy->disable_modern = true;
}

uint16_t virtio_pci_get_trans_devid(uint16_t device_id);
uint16_t virtio_pci_get_class_id(uint16_t device_id);

#define TYPE_VIRTIO_INPUT_PCI "virtio-input-pci"

#define VIRTIO_PCI_ABI_VERSION          0

typedef struct VirtioPCIDeviceTypeInfo {
    const char *base_name;
    const char *generic_name;
    const char *transitional_name;
    const char *non_transitional_name;

    const char *parent;

    size_t instance_size;
    size_t class_size;
    void (*instance_init)(Object *obj);
    void (*instance_finalize)(Object *obj);
    void (*class_init)(ObjectClass *klass, void *data);
    InterfaceInfo *interfaces;
} VirtioPCIDeviceTypeInfo;

void virtio_pci_types_register(const VirtioPCIDeviceTypeInfo *t);

unsigned virtio_pci_optimal_num_queues(unsigned fixed_queues);
void virtio_pci_set_guest_notifier_fd_handler(VirtIODevice *vdev, VirtQueue *vq,
                                              int n, bool assign,
                                              bool with_irqfd);

int virtio_pci_add_shm_cap(VirtIOPCIProxy *proxy, uint8_t bar, uint64_t offset,
                           uint64_t length, uint8_t id);

#endif
