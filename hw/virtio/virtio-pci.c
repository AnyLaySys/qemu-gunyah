

#include "qemu/osdep.h"

#include "exec/memop.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bus.h"
#include "hw/qdev-properties.h"
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-pci.h"
#include "hw/virtio/virtio.h"
#include "migration/qemu-file-types.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/range.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_pci.h"
#include "system/gunyah.h"
#include "system/gunyah_report.h"
#include "trace.h"

#define VIRTIO_PCI_REGION_SIZE(dev)     VIRTIO_PCI_CONFIG_OFF(msix_present(dev))

#undef VIRTIO_PCI_CONFIG


#define VIRTIO_PCI_CONFIG_SIZE(dev)     VIRTIO_PCI_CONFIG_OFF(msix_enabled(dev))

static void virtio_pci_bus_new(VirtioBusState *bus, size_t bus_size,
                               VirtIOPCIProxy *dev);
static void virtio_pci_reset(DeviceState *qdev);



static inline VirtIOPCIProxy *to_virtio_pci_proxy(DeviceState *d)
{
    return container_of(d, VirtIOPCIProxy, pci_dev.qdev);
}


static inline VirtIOPCIProxy *to_virtio_pci_proxy_fast(DeviceState *d)
{
    return container_of(d, VirtIOPCIProxy, pci_dev.qdev);
}

static void virtio_pci_notify(DeviceState *d, uint16_t vector)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy_fast(d);

    if (msix_enabled(&proxy->pci_dev)) {
        if (vector != VIRTIO_NO_VECTOR) {
            msix_notify(&proxy->pci_dev, vector);
        }
    } else if (!gunyah_enabled()) {
        VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
        pci_set_irq(&proxy->pci_dev, qatomic_read(&vdev->isr) & 1);
    }
}

static void virtio_pci_save_config(DeviceState *d, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    pci_device_save(&proxy->pci_dev, f);
    msix_save(&proxy->pci_dev, f);
    if (msix_present(&proxy->pci_dev))
        qemu_put_be16(f, vdev->config_vector);
}

static const VMStateDescription vmstate_virtio_pci_modern_queue_state = {
    .name = "virtio_pci/modern_queue_state",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT16(num, VirtIOPCIQueue),
        VMSTATE_UNUSED(1), 
        VMSTATE_BOOL(enabled, VirtIOPCIQueue),
        VMSTATE_UINT32_ARRAY(desc, VirtIOPCIQueue, 2),
        VMSTATE_UINT32_ARRAY(avail, VirtIOPCIQueue, 2),
        VMSTATE_UINT32_ARRAY(used, VirtIOPCIQueue, 2),
        VMSTATE_END_OF_LIST()
    }
};

static bool virtio_pci_modern_state_needed(void *opaque)
{
    VirtIOPCIProxy *proxy = opaque;

    return virtio_pci_modern(proxy);
}

static const VMStateDescription vmstate_virtio_pci_modern_state_sub = {
    .name = "virtio_pci/modern_state",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = &virtio_pci_modern_state_needed,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(dfselect, VirtIOPCIProxy),
        VMSTATE_UINT32(gfselect, VirtIOPCIProxy),
        VMSTATE_UINT32_ARRAY(guest_features, VirtIOPCIProxy, 2),
        VMSTATE_STRUCT_ARRAY(vqs, VirtIOPCIProxy, VIRTIO_QUEUE_MAX, 0,
                             vmstate_virtio_pci_modern_queue_state,
                             VirtIOPCIQueue),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_virtio_pci = {
    .name = "virtio_pci",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription * const []) {
        &vmstate_virtio_pci_modern_state_sub,
        NULL
    }
};

static bool virtio_pci_has_extra_state(DeviceState *d)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);

    return proxy->flags & VIRTIO_PCI_FLAG_MIGRATE_EXTRA;
}

static void virtio_pci_save_extra_state(DeviceState *d, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);

    vmstate_save_state(f, &vmstate_virtio_pci, proxy, NULL);
}

static int virtio_pci_load_extra_state(DeviceState *d, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);

    return vmstate_load_state(f, &vmstate_virtio_pci, proxy, 1);
}

static void virtio_pci_save_queue(DeviceState *d, int n, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (msix_present(&proxy->pci_dev))
        qemu_put_be16(f, virtio_queue_vector(vdev, n));
}

static int virtio_pci_load_config(DeviceState *d, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint16_t vector;

    int ret;
    ret = pci_device_load(&proxy->pci_dev, f);
    if (ret) {
        return ret;
    }
    msix_unuse_all_vectors(&proxy->pci_dev);
    msix_load(&proxy->pci_dev, f);
    if (msix_present(&proxy->pci_dev)) {
        qemu_get_be16s(f, &vector);

        if (vector != VIRTIO_NO_VECTOR && vector >= proxy->nvectors) {
            return -EINVAL;
        }
    } else {
        vector = VIRTIO_NO_VECTOR;
    }
    vdev->config_vector = vector;
    if (vector != VIRTIO_NO_VECTOR) {
        msix_vector_use(&proxy->pci_dev, vector);
    }
    return 0;
}

static int virtio_pci_load_queue(DeviceState *d, int n, QEMUFile *f)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    uint16_t vector;
    if (msix_present(&proxy->pci_dev)) {
        qemu_get_be16s(f, &vector);
        if (vector != VIRTIO_NO_VECTOR && vector >= proxy->nvectors) {
            return -EINVAL;
        }
    } else {
        vector = VIRTIO_NO_VECTOR;
    }
    virtio_queue_set_vector(vdev, n, vector);
    if (vector != VIRTIO_NO_VECTOR) {
        msix_vector_use(&proxy->pci_dev, vector);
    }

    return 0;
}

typedef struct VirtIOPCIIDInfo {
    
    uint16_t vdev_id;
    
    uint16_t trans_devid;
    uint16_t class_id;
} VirtIOPCIIDInfo;

static const VirtIOPCIIDInfo virtio_pci_id_info[] = {
    {
        .vdev_id = VIRTIO_ID_CRYPTO,
        .class_id = PCI_CLASS_OTHERS,
    }, {
        .vdev_id = VIRTIO_ID_FS,
        .class_id = PCI_CLASS_STORAGE_OTHER,
    }, {
        .vdev_id = VIRTIO_ID_NET,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_NET,
        .class_id = PCI_CLASS_NETWORK_ETHERNET,
    }, {
        .vdev_id = VIRTIO_ID_BLOCK,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_BLOCK,
        .class_id = PCI_CLASS_STORAGE_SCSI,
    }, {
        .vdev_id = VIRTIO_ID_CONSOLE,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_CONSOLE,
        .class_id = PCI_CLASS_COMMUNICATION_OTHER,
    }, {
        .vdev_id = VIRTIO_ID_SCSI,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_SCSI,
        .class_id = PCI_CLASS_STORAGE_SCSI
    }, {
        .vdev_id = VIRTIO_ID_9P,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_9P,
        .class_id = PCI_BASE_CLASS_NETWORK,
    }, {
        .vdev_id = VIRTIO_ID_BALLOON,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_BALLOON,
        .class_id = PCI_CLASS_OTHERS,
    }, {
        .vdev_id = VIRTIO_ID_RNG,
        .trans_devid = PCI_DEVICE_ID_VIRTIO_RNG,
        .class_id = PCI_CLASS_OTHERS,
    },
};

static const VirtIOPCIIDInfo *virtio_pci_get_id_info(uint16_t vdev_id)
{
    const VirtIOPCIIDInfo *info = NULL;
    int i;

    for (i = 0; i < ARRAY_SIZE(virtio_pci_id_info); i++) {
        if (virtio_pci_id_info[i].vdev_id == vdev_id) {
            info = &virtio_pci_id_info[i];
            break;
        }
    }

    if (!info) {
        
        error_report("Invalid virtio device(id %u)", vdev_id);
        abort();
    }

    return info;
}


uint16_t virtio_pci_get_trans_devid(uint16_t device_id)
{
    return virtio_pci_get_id_info(device_id)->trans_devid;
}


uint16_t virtio_pci_get_class_id(uint16_t device_id)
{
    return virtio_pci_get_id_info(device_id)->class_id;
}

static bool virtio_pci_ioeventfd_enabled(DeviceState *d)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);

    return (proxy->flags & VIRTIO_PCI_FLAG_USE_IOEVENTFD) != 0;
}

#define QEMU_VIRTIO_PCI_QUEUE_MEM_MULT 0x1000

static inline int virtio_pci_queue_mem_mult(struct VirtIOPCIProxy *proxy)
{
    return (proxy->flags & VIRTIO_PCI_FLAG_PAGE_PER_VQ) ?
        QEMU_VIRTIO_PCI_QUEUE_MEM_MULT : 4;
}

static int virtio_pci_ioeventfd_assign(DeviceState *d, EventNotifier *notifier,
                                       int n, bool assign)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtQueue *vq = virtio_get_queue(vdev, n);
    bool legacy = virtio_pci_legacy(proxy);
    bool modern = virtio_pci_modern(proxy);
    bool modern_pio = proxy->flags & VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY;
    MemoryRegion *modern_mr = &proxy->notify.mr;
    MemoryRegion *modern_notify_mr = &proxy->notify_pio.mr;
    MemoryRegion *legacy_mr = &proxy->bar;
    hwaddr modern_addr = virtio_pci_queue_mem_mult(proxy) *
                         virtio_get_queue_index(vq);
    hwaddr legacy_addr = VIRTIO_PCI_QUEUE_NOTIFY;

    if (assign) {
        if (modern) {
            memory_region_add_eventfd(modern_mr, modern_addr, 0,
                                      false, n, notifier);
            if (modern_pio) {
                memory_region_add_eventfd(modern_notify_mr, 0, 2,
                                              true, n, notifier);
            }
        }
        if (legacy) {
            memory_region_add_eventfd(legacy_mr, legacy_addr, 2,
                                      true, n, notifier);
        }
    } else {
        if (modern) {
            memory_region_del_eventfd(modern_mr, modern_addr, 0,
                                      false, n, notifier);
            if (modern_pio) {
                memory_region_del_eventfd(modern_notify_mr, 0, 2,
                                          true, n, notifier);
            }
        }
        if (legacy) {
            memory_region_del_eventfd(legacy_mr, legacy_addr, 2,
                                      true, n, notifier);
        }
    }
    return 0;
}

static void virtio_pci_start_ioeventfd(VirtIOPCIProxy *proxy)
{
    virtio_bus_start_ioeventfd(&proxy->bus);
}

static void virtio_pci_stop_ioeventfd(VirtIOPCIProxy *proxy)
{
    virtio_bus_stop_ioeventfd(&proxy->bus);
}

static void virtio_ioport_write(void *opaque, uint32_t addr, uint32_t val)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint16_t vector, vq_idx;
    hwaddr pa;

    switch (addr) {
    case VIRTIO_PCI_GUEST_FEATURES:
        
        if (val & (1 << VIRTIO_F_BAD_FEATURE)) {
            val = virtio_bus_get_vdev_bad_features(&proxy->bus);
        }
        virtio_set_features(vdev, val);
        break;
    case VIRTIO_PCI_QUEUE_PFN:
        pa = (hwaddr)val << VIRTIO_PCI_QUEUE_ADDR_SHIFT;
        if (pa == 0) {
            virtio_pci_reset(DEVICE(proxy));
        }
        else
            virtio_queue_set_addr(vdev, vdev->queue_sel, pa);
        break;
    case VIRTIO_PCI_QUEUE_SEL:
        if (val < VIRTIO_QUEUE_MAX)
            vdev->queue_sel = val;
        break;
    case VIRTIO_PCI_QUEUE_NOTIFY:
        vq_idx = val;
        if (vq_idx < VIRTIO_QUEUE_MAX && virtio_queue_get_num(vdev, vq_idx)) {
            if (virtio_vdev_has_feature(vdev, VIRTIO_F_NOTIFICATION_DATA)) {
                VirtQueue *vq = virtio_get_queue(vdev, vq_idx);

                virtio_queue_set_shadow_avail_idx(vq, val >> 16);
            }
            virtio_queue_notify(vdev, vq_idx);
        }
        break;
    case VIRTIO_PCI_STATUS:
        if (!(val & VIRTIO_CONFIG_S_DRIVER_OK)) {
            virtio_pci_stop_ioeventfd(proxy);
        }

        virtio_set_status(vdev, val & 0xFF);

        if (val & VIRTIO_CONFIG_S_DRIVER_OK) {
            virtio_pci_start_ioeventfd(proxy);
        }

        if (vdev->status == 0) {
            virtio_pci_reset(DEVICE(proxy));
        }

        
        if (val == (VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER)) {
            pci_default_write_config(&proxy->pci_dev, PCI_COMMAND,
                                     proxy->pci_dev.config[PCI_COMMAND] |
                                     PCI_COMMAND_MASTER, 1);
        }
        break;
    case VIRTIO_MSI_CONFIG_VECTOR:
        if (vdev->config_vector != VIRTIO_NO_VECTOR) {
            msix_vector_unuse(&proxy->pci_dev, vdev->config_vector);
        }
        
        if (val < proxy->nvectors) {
            msix_vector_use(&proxy->pci_dev, val);
        } else {
            val = VIRTIO_NO_VECTOR;
        }
        vdev->config_vector = val;
        break;
    case VIRTIO_MSI_QUEUE_VECTOR:
        vector = virtio_queue_vector(vdev, vdev->queue_sel);
        if (vector != VIRTIO_NO_VECTOR) {
            msix_vector_unuse(&proxy->pci_dev, vector);
        }
        
        if (val < proxy->nvectors) {
            msix_vector_use(&proxy->pci_dev, val);
        } else {
            val = VIRTIO_NO_VECTOR;
        }
        virtio_queue_set_vector(vdev, vdev->queue_sel, val);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: unexpected address 0x%x value 0x%x\n",
                      __func__, addr, val);
        break;
    }
}

static uint32_t virtio_ioport_read(VirtIOPCIProxy *proxy, uint32_t addr)
{
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint32_t ret = 0xFFFFFFFF;

    switch (addr) {
    case VIRTIO_PCI_HOST_FEATURES:
        ret = vdev->host_features;
        break;
    case VIRTIO_PCI_GUEST_FEATURES:
        ret = vdev->guest_features;
        break;
    case VIRTIO_PCI_QUEUE_PFN:
        ret = virtio_queue_get_addr(vdev, vdev->queue_sel)
              >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
        break;
    case VIRTIO_PCI_QUEUE_NUM:
        ret = virtio_queue_get_num(vdev, vdev->queue_sel);
        break;
    case VIRTIO_PCI_QUEUE_SEL:
        ret = vdev->queue_sel;
        break;
    case VIRTIO_PCI_STATUS:
        ret = vdev->status;
        break;
    case VIRTIO_PCI_ISR:
        
        ret = qatomic_xchg(&vdev->isr, 0);
        pci_irq_deassert(&proxy->pci_dev);
        break;
    case VIRTIO_MSI_CONFIG_VECTOR:
        ret = vdev->config_vector;
        break;
    case VIRTIO_MSI_QUEUE_VECTOR:
        ret = virtio_queue_vector(vdev, vdev->queue_sel);
        break;
    default:
        break;
    }

    return ret;
}

static uint64_t virtio_pci_config_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint32_t config = VIRTIO_PCI_CONFIG_SIZE(&proxy->pci_dev);
    uint64_t val = 0;

    if (vdev == NULL) {
        return UINT64_MAX;
    }

    if (addr < config) {
        return virtio_ioport_read(proxy, addr);
    }
    addr -= config;

    switch (size) {
    case 1:
        val = virtio_config_readb(vdev, addr);
        break;
    case 2:
        val = virtio_config_readw(vdev, addr);
        if (virtio_is_big_endian(vdev)) {
            val = bswap16(val);
        }
        break;
    case 4:
        val = virtio_config_readl(vdev, addr);
        if (virtio_is_big_endian(vdev)) {
            val = bswap32(val);
        }
        break;
    }
    return val;
}

static void virtio_pci_config_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    uint32_t config = VIRTIO_PCI_CONFIG_SIZE(&proxy->pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (vdev == NULL) {
        return;
    }

    if (addr < config) {
        virtio_ioport_write(proxy, addr, val);
        return;
    }
    addr -= config;
    
    switch (size) {
    case 1:
        virtio_config_writeb(vdev, addr, val);
        break;
    case 2:
        if (virtio_is_big_endian(vdev)) {
            val = bswap16(val);
        }
        virtio_config_writew(vdev, addr, val);
        break;
    case 4:
        if (virtio_is_big_endian(vdev)) {
            val = bswap32(val);
        }
        virtio_config_writel(vdev, addr, val);
        break;
    }
}

static const MemoryRegionOps virtio_pci_config_ops = {
    .read = virtio_pci_config_read,
    .write = virtio_pci_config_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static MemoryRegion *virtio_address_space_lookup(VirtIOPCIProxy *proxy,
                                                 hwaddr *off, int len)
{
    int i;
    VirtIOPCIRegion *reg;

    for (i = 0; i < ARRAY_SIZE(proxy->regs); ++i) {
        reg = &proxy->regs[i];
        if (*off >= reg->offset &&
            *off + len <= reg->offset + reg->size) {
            MemoryRegionSection mrs = memory_region_find(&reg->mr,
                                        *off - reg->offset, len);
            assert(mrs.mr);
            *off = mrs.offset_within_region;
            memory_region_unref(mrs.mr);
            return mrs.mr;
        }
    }

    return NULL;
}


static
void virtio_address_space_write(VirtIOPCIProxy *proxy, hwaddr addr,
                                const uint8_t *buf, int len)
{
    uint64_t val;
    MemoryRegion *mr;

    
    addr &= ~(len - 1);

    mr = virtio_address_space_lookup(proxy, &addr, len);
    if (!mr) {
        return;
    }

    
    assert(!(((uintptr_t)buf) & (len - 1)));

    switch (len) {
    case 1:
        val = pci_get_byte(buf);
        break;
    case 2:
        val = pci_get_word(buf);
        break;
    case 4:
        val = pci_get_long(buf);
        break;
    default:
        
        return;
    }
    memory_region_dispatch_write(mr, addr, val, size_memop(len) | MO_LE,
                                 MEMTXATTRS_UNSPECIFIED);
}

static void
virtio_address_space_read(VirtIOPCIProxy *proxy, hwaddr addr,
                          uint8_t *buf, int len)
{
    uint64_t val;
    MemoryRegion *mr;

    
    addr &= ~(len - 1);

    mr = virtio_address_space_lookup(proxy, &addr, len);
    if (!mr) {
        return;
    }

    
    assert(!(((uintptr_t)buf) & (len - 1)));

    memory_region_dispatch_read(mr, addr, &val, size_memop(len) | MO_LE,
                                MEMTXATTRS_UNSPECIFIED);
    switch (len) {
    case 1:
        pci_set_byte(buf, val);
        break;
    case 2:
        pci_set_word(buf, val);
        break;
    case 4:
        pci_set_long(buf, val);
        break;
    default:
        
        break;
    }
}

static void virtio_pci_ats_ctrl_trigger(PCIDevice *pci_dev, bool enable)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *k = VIRTIO_DEVICE_GET_CLASS(vdev);

    vdev->device_iotlb_enabled = enable;

    if (k->toggle_device_iotlb) {
        k->toggle_device_iotlb(vdev);
    }
}

static void pcie_ats_config_write(PCIDevice *dev, uint32_t address,
                                  uint32_t val, int len)
{
    uint32_t off;
    uint16_t ats_cap = dev->exp.ats_cap;

    if (!ats_cap || address < ats_cap) {
        return;
    }
    off = address - ats_cap;
    if (off >= PCI_EXT_CAP_ATS_SIZEOF) {
        return;
    }

    if (range_covers_byte(off, len, PCI_ATS_CTRL + 1)) {
        virtio_pci_ats_ctrl_trigger(dev, !!(val & PCI_ATS_CTRL_ENABLE));
    }
}

static void virtio_write_config(PCIDevice *pci_dev, uint32_t address,
                                uint32_t val, int len)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    struct virtio_pci_cfg_cap *cfg;

    pci_default_write_config(pci_dev, address, val, len);

    if (proxy->flags & VIRTIO_PCI_FLAG_INIT_FLR) {
        pcie_cap_flr_write_config(pci_dev, address, val, len);
    }

    if (proxy->flags & VIRTIO_PCI_FLAG_ATS) {
        pcie_ats_config_write(pci_dev, address, val, len);
    }

    if (range_covers_byte(address, len, PCI_COMMAND)) {
        if (!(pci_dev->config[PCI_COMMAND] & PCI_COMMAND_MASTER)) {
            virtio_set_disabled(vdev, true);
            virtio_pci_stop_ioeventfd(proxy);
            virtio_set_status(vdev, vdev->status & ~VIRTIO_CONFIG_S_DRIVER_OK);
        } else {
            virtio_set_disabled(vdev, false);
        }
    }

    if (proxy->config_cap &&
        ranges_overlap(address, len, proxy->config_cap + offsetof(struct virtio_pci_cfg_cap,
                                                                  pci_cfg_data),
                       sizeof cfg->pci_cfg_data)) {
        uint32_t off;
        uint32_t caplen;

        cfg = (void *)(proxy->pci_dev.config + proxy->config_cap);
        off = le32_to_cpu(cfg->cap.offset);
        caplen = le32_to_cpu(cfg->cap.length);

        if (caplen == 1 || caplen == 2 || caplen == 4) {
            assert(caplen <= sizeof cfg->pci_cfg_data);
            virtio_address_space_write(proxy, off, cfg->pci_cfg_data, caplen);
        }
    }
}

static uint32_t virtio_read_config(PCIDevice *pci_dev,
                                   uint32_t address, int len)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(pci_dev);
    struct virtio_pci_cfg_cap *cfg;

    if (proxy->config_cap &&
        ranges_overlap(address, len, proxy->config_cap + offsetof(struct virtio_pci_cfg_cap,
                                                                  pci_cfg_data),
                       sizeof cfg->pci_cfg_data)) {
        uint32_t off;
        uint32_t caplen;

        cfg = (void *)(proxy->pci_dev.config + proxy->config_cap);
        off = le32_to_cpu(cfg->cap.offset);
        caplen = le32_to_cpu(cfg->cap.length);

        if (caplen == 1 || caplen == 2 || caplen == 4) {
            assert(caplen <= sizeof cfg->pci_cfg_data);
            virtio_address_space_read(proxy, off, cfg->pci_cfg_data, caplen);
        }
    }

    return pci_default_read_config(pci_dev, address, len);
}

static int virtio_pci_get_notifier(VirtIOPCIProxy *proxy, int queue_no,
                                      EventNotifier **n, unsigned int *vector)
{
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtQueue *vq;

    if (!proxy->vector_irqfd && vdev->status & VIRTIO_CONFIG_S_DRIVER_OK)
        return -1;

    if (queue_no == VIRTIO_CONFIG_IRQ_IDX) {
        *n = virtio_config_get_guest_notifier(vdev);
        *vector = vdev->config_vector;
    } else {
        if (!virtio_queue_get_num(vdev, queue_no)) {
            return -1;
        }
        *vector = virtio_queue_vector(vdev, queue_no);
        vq = virtio_get_queue(vdev, queue_no);
        *n = virtio_queue_get_guest_notifier(vq);
    }
    return 0;
}

static int virtio_pci_one_vector_unmask(VirtIOPCIProxy *proxy,
                                       unsigned int queue_no,
                                       unsigned int vector,
                                       MSIMessage msg,
                                       EventNotifier *n)
{
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *k = VIRTIO_DEVICE_GET_CLASS(vdev);

    if (vdev->use_guest_notifier_mask && k->guest_notifier_mask) {
        k->guest_notifier_mask(vdev, queue_no, false);
        if (k->guest_notifier_pending &&
            k->guest_notifier_pending(vdev, queue_no)) {
            event_notifier_set(n);
        }
    }
    return 0;
}

static void virtio_pci_one_vector_mask(VirtIOPCIProxy *proxy,
                                             unsigned int queue_no,
                                             unsigned int vector,
                                             EventNotifier *n)
{
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *k = VIRTIO_DEVICE_GET_CLASS(vdev);

    if (vdev->use_guest_notifier_mask && k->guest_notifier_mask) {
        k->guest_notifier_mask(vdev, queue_no, true);
    }
}

static int virtio_pci_vector_unmask(PCIDevice *dev, unsigned vector,
                                    MSIMessage msg)
{
    VirtIOPCIProxy *proxy = container_of(dev, VirtIOPCIProxy, pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtQueue *vq = virtio_vector_first_queue(vdev, vector);
    EventNotifier *n;
    int ret, index, unmasked = 0;

    while (vq) {
        index = virtio_get_queue_index(vq);
        if (!virtio_queue_get_num(vdev, index)) {
            break;
        }
        if (index < proxy->nvqs_with_notifiers) {
            n = virtio_queue_get_guest_notifier(vq);
            ret = virtio_pci_one_vector_unmask(proxy, index, vector, msg, n);
            if (ret < 0) {
                goto undo;
            }
            ++unmasked;
        }
        vq = virtio_vector_next_queue(vq);
    }
    
    if (vector == vdev->config_vector) {
        n = virtio_config_get_guest_notifier(vdev);
        ret = virtio_pci_one_vector_unmask(proxy, VIRTIO_CONFIG_IRQ_IDX, vector,
                                           msg, n);
        if (ret < 0) {
            goto undo_config;
        }
    }
    return 0;
undo_config:
    n = virtio_config_get_guest_notifier(vdev);
    virtio_pci_one_vector_mask(proxy, VIRTIO_CONFIG_IRQ_IDX, vector, n);
undo:
    vq = virtio_vector_first_queue(vdev, vector);
    while (vq && unmasked >= 0) {
        index = virtio_get_queue_index(vq);
        if (index < proxy->nvqs_with_notifiers) {
            n = virtio_queue_get_guest_notifier(vq);
            virtio_pci_one_vector_mask(proxy, index, vector, n);
            --unmasked;
        }
        vq = virtio_vector_next_queue(vq);
    }
    return ret;
}

static void virtio_pci_vector_mask(PCIDevice *dev, unsigned vector)
{
    VirtIOPCIProxy *proxy = container_of(dev, VirtIOPCIProxy, pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtQueue *vq = virtio_vector_first_queue(vdev, vector);
    EventNotifier *n;
    int index;

    while (vq) {
        index = virtio_get_queue_index(vq);
        n = virtio_queue_get_guest_notifier(vq);
        if (!virtio_queue_get_num(vdev, index)) {
            break;
        }
        if (index < proxy->nvqs_with_notifiers) {
            virtio_pci_one_vector_mask(proxy, index, vector, n);
        }
        vq = virtio_vector_next_queue(vq);
    }

    if (vector == vdev->config_vector) {
        n = virtio_config_get_guest_notifier(vdev);
        virtio_pci_one_vector_mask(proxy, VIRTIO_CONFIG_IRQ_IDX, vector, n);
    }
}

static void virtio_pci_vector_poll(PCIDevice *dev,
                                   unsigned int vector_start,
                                   unsigned int vector_end)
{
    VirtIOPCIProxy *proxy = container_of(dev, VirtIOPCIProxy, pci_dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *k = VIRTIO_DEVICE_GET_CLASS(vdev);
    int queue_no;
    unsigned int vector;
    EventNotifier *notifier;
    int ret;

    for (queue_no = 0; queue_no < proxy->nvqs_with_notifiers; queue_no++) {
        ret = virtio_pci_get_notifier(proxy, queue_no, &notifier, &vector);
        if (ret < 0) {
            break;
        }
        if (vector < vector_start || vector >= vector_end ||
            !msix_is_masked(dev, vector)) {
            continue;
        }
        if (k->guest_notifier_pending) {
            if (k->guest_notifier_pending(vdev, queue_no)) {
                msix_set_pending(dev, vector);
            }
        } else if (event_notifier_test_and_clear(notifier)) {
            msix_set_pending(dev, vector);
        }
    }
    
    ret = virtio_pci_get_notifier(proxy, VIRTIO_CONFIG_IRQ_IDX, &notifier,
                                  &vector);
    if (ret < 0) {
        return;
    }
    if (vector < vector_start || vector >= vector_end ||
        !msix_is_masked(dev, vector)) {
        return;
    }
    if (k->guest_notifier_pending) {
        if (k->guest_notifier_pending(vdev, VIRTIO_CONFIG_IRQ_IDX)) {
            msix_set_pending(dev, vector);
        }
    } else if (event_notifier_test_and_clear(notifier)) {
        msix_set_pending(dev, vector);
    }
}

void virtio_pci_set_guest_notifier_fd_handler(VirtIODevice *vdev, VirtQueue *vq,
                                              int n, bool assign,
                                              bool with_irqfd)
{
    if (n == VIRTIO_CONFIG_IRQ_IDX) {
        virtio_config_set_guest_notifier_fd_handler(vdev, assign, with_irqfd);
    } else {
        virtio_queue_set_guest_notifier_fd_handler(vq, assign, with_irqfd);
    }
}

static int virtio_pci_set_guest_notifier(DeviceState *d, int n, bool assign,
                                         bool with_irqfd)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_GET_CLASS(vdev);
    VirtQueue *vq = NULL;
    EventNotifier *notifier = NULL;

    if (n == VIRTIO_CONFIG_IRQ_IDX) {
        notifier = virtio_config_get_guest_notifier(vdev);
    } else {
        vq = virtio_get_queue(vdev, n);
        notifier = virtio_queue_get_guest_notifier(vq);
    }

    if (assign) {
        int r = event_notifier_init(notifier, 0);
        if (r < 0) {
            return r;
        }
        virtio_pci_set_guest_notifier_fd_handler(vdev, vq, n, true, with_irqfd);
    } else {
        virtio_pci_set_guest_notifier_fd_handler(vdev, vq, n, false,
                                                 with_irqfd);
        event_notifier_cleanup(notifier);
    }

    if (!msix_enabled(&proxy->pci_dev) &&
        vdev->use_guest_notifier_mask &&
        vdc->guest_notifier_mask) {
        vdc->guest_notifier_mask(vdev, n, !assign);
    }

    return 0;
}

static bool virtio_pci_query_guest_notifiers(DeviceState *d)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    return msix_enabled(&proxy->pci_dev);
}

static int virtio_pci_set_guest_notifiers(DeviceState *d, int nvqs, bool assign)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    VirtioDeviceClass *k = VIRTIO_DEVICE_GET_CLASS(vdev);
    int r, n;
    bool with_irqfd = false;

    nvqs = MIN(nvqs, VIRTIO_QUEUE_MAX);

    
    if (!assign && !proxy->nvqs_with_notifiers) {
        return 0;
    }
    assert(assign || nvqs == proxy->nvqs_with_notifiers);

    proxy->nvqs_with_notifiers = nvqs;

    
    if ((proxy->vector_irqfd ||
         (vdev->use_guest_notifier_mask && k->guest_notifier_mask)) &&
        !assign) {
        msix_unset_vector_notifiers(&proxy->pci_dev);
        if (proxy->vector_irqfd) {
            g_free(proxy->vector_irqfd);
            proxy->vector_irqfd = NULL;
        }
    }

    for (n = 0; n < nvqs; n++) {
        if (!virtio_queue_get_num(vdev, n)) {
            break;
        }

        r = virtio_pci_set_guest_notifier(d, n, assign, with_irqfd);
        if (r < 0) {
            goto assign_error;
        }
    }
    r = virtio_pci_set_guest_notifier(d, VIRTIO_CONFIG_IRQ_IDX, assign,
                                      with_irqfd);
    if (r < 0) {
        goto config_assign_error;
    }
    
    if ((with_irqfd ||
         (vdev->use_guest_notifier_mask && k->guest_notifier_mask)) &&
        assign) {
        if (with_irqfd) {
            proxy->vector_irqfd =
                g_malloc0(sizeof(*proxy->vector_irqfd) *
                          msix_nr_vectors_allocated(&proxy->pci_dev));
        }

        r = msix_set_vector_notifiers(&proxy->pci_dev, virtio_pci_vector_unmask,
                                      virtio_pci_vector_mask,
                                      virtio_pci_vector_poll);
        if (r < 0) {
            goto notifiers_error;
        }
    }

    return 0;

notifiers_error:
    if (with_irqfd) {
        assert(assign);
    }
config_assign_error:
    virtio_pci_set_guest_notifier(d, VIRTIO_CONFIG_IRQ_IDX, !assign,
                                  with_irqfd);
assign_error:
    
    assert(assign);
    while (--n >= 0) {
        virtio_pci_set_guest_notifier(d, n, !assign, with_irqfd);
    }
    g_free(proxy->vector_irqfd);
    proxy->vector_irqfd = NULL;
    return r;
}

static int virtio_pci_set_host_notifier_mr(DeviceState *d, int n,
                                           MemoryRegion *mr, bool assign)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    int offset;

    if (n >= VIRTIO_QUEUE_MAX || !virtio_pci_modern(proxy) ||
        virtio_pci_queue_mem_mult(proxy) != memory_region_size(mr)) {
        return -1;
    }

    if (assign) {
        offset = virtio_pci_queue_mem_mult(proxy) * n;
        memory_region_add_subregion_overlap(&proxy->notify.mr, offset, mr, 1);
    } else {
        memory_region_del_subregion(&proxy->notify.mr, mr);
    }

    return 0;
}

static void virtio_pci_vmstate_change(DeviceState *d, bool running)
{
    VirtIOPCIProxy *proxy = to_virtio_pci_proxy(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (running) {
        
        if ((proxy->flags & VIRTIO_PCI_FLAG_BUS_MASTER_BUG_MIGRATION) &&
            (vdev->status & VIRTIO_CONFIG_S_DRIVER) &&
            !(proxy->pci_dev.config[PCI_COMMAND] & PCI_COMMAND_MASTER)) {
            pci_default_write_config(&proxy->pci_dev, PCI_COMMAND,
                                     proxy->pci_dev.config[PCI_COMMAND] |
                                     PCI_COMMAND_MASTER, 1);
        }
        virtio_pci_start_ioeventfd(proxy);
    } else {
        virtio_pci_stop_ioeventfd(proxy);
    }
}



static int virtio_pci_query_nvectors(DeviceState *d)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);

    return proxy->nvectors;
}

static AddressSpace *virtio_pci_get_dma_as(DeviceState *d)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    PCIDevice *dev = &proxy->pci_dev;

    return pci_get_address_space(dev);
}

static bool virtio_pci_iommu_enabled(DeviceState *d)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    PCIDevice *dev = &proxy->pci_dev;
    AddressSpace *dma_as = pci_device_iommu_address_space(dev);

    if (dma_as == &address_space_memory) {
        return false;
    }

    return true;
}

static bool virtio_pci_queue_enabled(DeviceState *d, int n)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (virtio_vdev_has_feature(vdev, VIRTIO_F_VERSION_1)) {
        return proxy->vqs[n].enabled;
    }

    return virtio_queue_enabled_legacy(vdev, n);
}

static int virtio_pci_add_mem_cap(VirtIOPCIProxy *proxy,
                                   struct virtio_pci_cap *cap)
{
    PCIDevice *dev = &proxy->pci_dev;
    int offset;

    offset = pci_add_capability(dev, PCI_CAP_ID_VNDR, 0,
                                cap->cap_len, &error_abort);

    assert(cap->cap_len >= sizeof *cap);
    memcpy(dev->config + offset + PCI_CAP_FLAGS, &cap->cap_len,
           cap->cap_len - PCI_CAP_FLAGS);

    return offset;
}

static void virtio_pci_set_vector(VirtIODevice *vdev,
                                  VirtIOPCIProxy *proxy,
                                  int queue_no, uint16_t old_vector,
                                  uint16_t new_vector)
{
    if (new_vector == old_vector) {
        return;
    }

    if (queue_no == VIRTIO_CONFIG_IRQ_IDX) {
        vdev->config_vector = new_vector;
    } else {
        virtio_queue_set_vector(vdev, queue_no, new_vector);
    }
}

int virtio_pci_add_shm_cap(VirtIOPCIProxy *proxy,
                           uint8_t bar, uint64_t offset, uint64_t length,
                           uint8_t id)
{
    struct virtio_pci_cap64 cap = {
        .cap.cap_len = sizeof cap,
        .cap.cfg_type = VIRTIO_PCI_CAP_SHARED_MEMORY_CFG,
    };

    cap.cap.bar = bar;
    cap.cap.length = cpu_to_le32(length);
    cap.length_hi = cpu_to_le32(length >> 32);
    cap.cap.offset = cpu_to_le32(offset);
    cap.offset_hi = cpu_to_le32(offset >> 32);
    cap.cap.id = id;
    return virtio_pci_add_mem_cap(proxy, &cap.cap);
}

static uint64_t virtio_pci_common_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint32_t val = 0;
    int i;

    if (vdev == NULL) {
        return UINT64_MAX;
    }

    switch (addr) {
    case VIRTIO_PCI_COMMON_DFSELECT:
        val = proxy->dfselect;
        break;
    case VIRTIO_PCI_COMMON_DF:
        if (proxy->dfselect <= 1) {
            VirtioDeviceClass *vdc = VIRTIO_DEVICE_GET_CLASS(vdev);

            val = (vdev->host_features & ~vdc->legacy_features) >>
                (32 * proxy->dfselect);
        }
        break;
    case VIRTIO_PCI_COMMON_GFSELECT:
        val = proxy->gfselect;
        break;
    case VIRTIO_PCI_COMMON_GF:
        if (proxy->gfselect < ARRAY_SIZE(proxy->guest_features)) {
            val = proxy->guest_features[proxy->gfselect];
        }
        break;
    case VIRTIO_PCI_COMMON_MSIX:
        val = vdev->config_vector;
        break;
    case VIRTIO_PCI_COMMON_NUMQ:
        for (i = 0; i < VIRTIO_QUEUE_MAX; ++i) {
            if (virtio_queue_get_num(vdev, i)) {
                val = i + 1;
            }
        }
        break;
    case VIRTIO_PCI_COMMON_STATUS:
        val = vdev->status;
        break;
    case VIRTIO_PCI_COMMON_CFGGENERATION:
        val = vdev->generation;
        break;
    case VIRTIO_PCI_COMMON_Q_SELECT:
        val = vdev->queue_sel;
        break;
    case VIRTIO_PCI_COMMON_Q_SIZE:
        val = virtio_queue_get_num(vdev, vdev->queue_sel);
        break;
    case VIRTIO_PCI_COMMON_Q_MSIX:
        val = virtio_queue_vector(vdev, vdev->queue_sel);
        break;
    case VIRTIO_PCI_COMMON_Q_ENABLE:
        val = proxy->vqs[vdev->queue_sel].enabled;
        break;
    case VIRTIO_PCI_COMMON_Q_NOFF:
        
        val = vdev->queue_sel;
        break;
    case VIRTIO_PCI_COMMON_Q_DESCLO:
        val = proxy->vqs[vdev->queue_sel].desc[0];
        break;
    case VIRTIO_PCI_COMMON_Q_DESCHI:
        val = proxy->vqs[vdev->queue_sel].desc[1];
        break;
    case VIRTIO_PCI_COMMON_Q_AVAILLO:
        val = proxy->vqs[vdev->queue_sel].avail[0];
        break;
    case VIRTIO_PCI_COMMON_Q_AVAILHI:
        val = proxy->vqs[vdev->queue_sel].avail[1];
        break;
    case VIRTIO_PCI_COMMON_Q_USEDLO:
        val = proxy->vqs[vdev->queue_sel].used[0];
        break;
    case VIRTIO_PCI_COMMON_Q_USEDHI:
        val = proxy->vqs[vdev->queue_sel].used[1];
        break;
    case VIRTIO_PCI_COMMON_Q_RESET:
        val = proxy->vqs[vdev->queue_sel].reset;
        break;
    default:
        val = 0;
    }

    return val;
}

static void virtio_pci_common_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint16_t vector;

    if (vdev == NULL) {
        return;
    }

    switch (addr) {
    case VIRTIO_PCI_COMMON_DFSELECT:
        proxy->dfselect = val;
        break;
    case VIRTIO_PCI_COMMON_GFSELECT:
        proxy->gfselect = val;
        break;
    case VIRTIO_PCI_COMMON_GF:
        if (proxy->gfselect < ARRAY_SIZE(proxy->guest_features)) {
            proxy->guest_features[proxy->gfselect] = val;
            virtio_set_features(vdev,
                                (((uint64_t)proxy->guest_features[1]) << 32) |
                                proxy->guest_features[0]);
        }
        break;
    case VIRTIO_PCI_COMMON_MSIX:
        if (vdev->config_vector != VIRTIO_NO_VECTOR) {
            msix_vector_unuse(&proxy->pci_dev, vdev->config_vector);
        }
        
        if (val < proxy->nvectors) {
            msix_vector_use(&proxy->pci_dev, val);
        } else {
            val = VIRTIO_NO_VECTOR;
        }
        virtio_pci_set_vector(vdev, proxy, VIRTIO_CONFIG_IRQ_IDX,
                              vdev->config_vector, val);
        break;
    case VIRTIO_PCI_COMMON_STATUS:
        if (!(val & VIRTIO_CONFIG_S_DRIVER_OK)) {
            virtio_pci_stop_ioeventfd(proxy);
        }

        virtio_set_status(vdev, val & 0xFF);

        if (val & VIRTIO_CONFIG_S_DRIVER_OK) {
            virtio_pci_start_ioeventfd(proxy);
        }

        if (vdev->status == 0) {
            virtio_pci_reset(DEVICE(proxy));
        }

        break;
    case VIRTIO_PCI_COMMON_Q_SELECT:
        if (val < VIRTIO_QUEUE_MAX) {
            vdev->queue_sel = val;
        }
        break;
    case VIRTIO_PCI_COMMON_Q_SIZE:
        proxy->vqs[vdev->queue_sel].num = val;
        virtio_queue_set_num(vdev, vdev->queue_sel,
                             proxy->vqs[vdev->queue_sel].num);
        virtio_init_region_cache(vdev, vdev->queue_sel);
        break;
    case VIRTIO_PCI_COMMON_Q_MSIX:
        vector = virtio_queue_vector(vdev, vdev->queue_sel);
        if (vector != VIRTIO_NO_VECTOR) {
            msix_vector_unuse(&proxy->pci_dev, vector);
        }
        
        if (val < proxy->nvectors) {
            msix_vector_use(&proxy->pci_dev, val);
        } else {
            val = VIRTIO_NO_VECTOR;
        }
        virtio_pci_set_vector(vdev, proxy, vdev->queue_sel, vector, val);
        break;
    case VIRTIO_PCI_COMMON_Q_ENABLE:
        if (val == 1) {
            virtio_queue_set_num(vdev, vdev->queue_sel,
                                 proxy->vqs[vdev->queue_sel].num);
            virtio_queue_set_rings(vdev, vdev->queue_sel,
                       ((uint64_t)proxy->vqs[vdev->queue_sel].desc[1]) << 32 |
                       proxy->vqs[vdev->queue_sel].desc[0],
                       ((uint64_t)proxy->vqs[vdev->queue_sel].avail[1]) << 32 |
                       proxy->vqs[vdev->queue_sel].avail[0],
                       ((uint64_t)proxy->vqs[vdev->queue_sel].used[1]) << 32 |
                       proxy->vqs[vdev->queue_sel].used[0]);
            proxy->vqs[vdev->queue_sel].enabled = 1;
            proxy->vqs[vdev->queue_sel].reset = 0;
            virtio_queue_enable(vdev, vdev->queue_sel);
        } else {
            virtio_error(vdev, "wrong value for queue_enable %"PRIx64, val);
        }
        break;
    case VIRTIO_PCI_COMMON_Q_DESCLO:
        proxy->vqs[vdev->queue_sel].desc[0] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_DESCHI:
        proxy->vqs[vdev->queue_sel].desc[1] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_AVAILLO:
        proxy->vqs[vdev->queue_sel].avail[0] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_AVAILHI:
        proxy->vqs[vdev->queue_sel].avail[1] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_USEDLO:
        proxy->vqs[vdev->queue_sel].used[0] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_USEDHI:
        proxy->vqs[vdev->queue_sel].used[1] = val;
        break;
    case VIRTIO_PCI_COMMON_Q_RESET:
        if (val == 1) {
            proxy->vqs[vdev->queue_sel].reset = 1;

            virtio_queue_reset(vdev, vdev->queue_sel);

            proxy->vqs[vdev->queue_sel].reset = 0;
            proxy->vqs[vdev->queue_sel].enabled = 0;
        }
        break;
    default:
        break;
    }
}


static uint64_t virtio_pci_notify_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    if (virtio_bus_get_device(&proxy->bus) == NULL) {
        return UINT64_MAX;
    }

    return 0;
}

static void virtio_pci_notify_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    unsigned queue = addr / virtio_pci_queue_mem_mult(proxy);

    if (vdev != NULL && queue < VIRTIO_QUEUE_MAX) {
        trace_virtio_pci_notify_write(addr, val, size);
        virtio_queue_notify(vdev, queue);
    }
}

static void virtio_pci_notify_write_pio(void *opaque, hwaddr addr,
                                        uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    unsigned queue = val;

    if (vdev != NULL && queue < VIRTIO_QUEUE_MAX) {
        trace_virtio_pci_notify_write_pio(addr, val, size);
        virtio_queue_notify(vdev, queue);
    }
}

static uint64_t virtio_pci_isr_read(void *opaque, hwaddr addr,
                                    unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint64_t val;

    if (vdev == NULL) {
        return UINT64_MAX;
    }

    val = qatomic_xchg(&vdev->isr, 0);
    pci_irq_deassert(&proxy->pci_dev);
    return val;
}

static void virtio_pci_isr_write(void *opaque, hwaddr addr,
                                 uint64_t val, unsigned size)
{
}

static uint64_t virtio_pci_device_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);
    uint64_t val;

    if (vdev == NULL) {
        return UINT64_MAX;
    }

    switch (size) {
    case 1:
        val = virtio_config_modern_readb(vdev, addr);
        break;
    case 2:
        val = virtio_config_modern_readw(vdev, addr);
        break;
    case 4:
        val = virtio_config_modern_readl(vdev, addr);
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

static void virtio_pci_device_write(void *opaque, hwaddr addr,
                                    uint64_t val, unsigned size)
{
    VirtIOPCIProxy *proxy = opaque;
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (vdev == NULL) {
        return;
    }

    switch (size) {
    case 1:
        virtio_config_modern_writeb(vdev, addr, val);
        break;
    case 2:
        virtio_config_modern_writew(vdev, addr, val);
        break;
    case 4:
        virtio_config_modern_writel(vdev, addr, val);
        break;
    }
}

static void virtio_pci_modern_regions_init(VirtIOPCIProxy *proxy,
                                           const char *vdev_name)
{
    static const MemoryRegionOps common_ops = {
        .read = virtio_pci_common_read,
        .write = virtio_pci_common_write,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 4,
        },
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    static const MemoryRegionOps isr_ops = {
        .read = virtio_pci_isr_read,
        .write = virtio_pci_isr_write,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 4,
        },
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    static const MemoryRegionOps device_ops = {
        .read = virtio_pci_device_read,
        .write = virtio_pci_device_write,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 4,
        },
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    static const MemoryRegionOps notify_ops = {
        .read = virtio_pci_notify_read,
        .write = virtio_pci_notify_write,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 4,
        },
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    static const MemoryRegionOps notify_pio_ops = {
        .read = virtio_pci_notify_read,
        .write = virtio_pci_notify_write_pio,
        .impl = {
            .min_access_size = 1,
            .max_access_size = 4,
        },
        .endianness = DEVICE_LITTLE_ENDIAN,
    };
    g_autoptr(GString) name = g_string_new(NULL);

    g_string_printf(name, "virtio-pci-common-%s", vdev_name);
    memory_region_init_io(&proxy->common.mr, OBJECT(proxy),
                          &common_ops,
                          proxy,
                          name->str,
                          proxy->common.size);

    g_string_printf(name, "virtio-pci-isr-%s", vdev_name);
    memory_region_init_io(&proxy->isr.mr, OBJECT(proxy),
                          &isr_ops,
                          proxy,
                          name->str,
                          proxy->isr.size);

    g_string_printf(name, "virtio-pci-device-%s", vdev_name);
    memory_region_init_io(&proxy->device.mr, OBJECT(proxy),
                          &device_ops,
                          proxy,
                          name->str,
                          proxy->device.size);

    g_string_printf(name, "virtio-pci-notify-%s", vdev_name);
    memory_region_init_io(&proxy->notify.mr, OBJECT(proxy),
                          &notify_ops,
                          proxy,
                          name->str,
                          proxy->notify.size);

    g_string_printf(name, "virtio-pci-notify-pio-%s", vdev_name);
    memory_region_init_io(&proxy->notify_pio.mr, OBJECT(proxy),
                          &notify_pio_ops,
                          proxy,
                          name->str,
                          proxy->notify_pio.size);
}

static void virtio_pci_modern_region_map(VirtIOPCIProxy *proxy,
                                         VirtIOPCIRegion *region,
                                         struct virtio_pci_cap *cap,
                                         MemoryRegion *mr,
                                         uint8_t bar)
{
    memory_region_add_subregion(mr, region->offset, &region->mr);

    cap->cfg_type = region->type;
    cap->bar = bar;
    cap->offset = cpu_to_le32(region->offset);
    cap->length = cpu_to_le32(region->size);
    virtio_pci_add_mem_cap(proxy, cap);

}

static void virtio_pci_modern_mem_region_map(VirtIOPCIProxy *proxy,
                                             VirtIOPCIRegion *region,
                                             struct virtio_pci_cap *cap)
{
    virtio_pci_modern_region_map(proxy, region, cap,
                                 &proxy->modern_bar, proxy->modern_mem_bar_idx);
}

static void virtio_pci_modern_io_region_map(VirtIOPCIProxy *proxy,
                                            VirtIOPCIRegion *region,
                                            struct virtio_pci_cap *cap)
{
    virtio_pci_modern_region_map(proxy, region, cap,
                                 &proxy->io_bar, proxy->modern_io_bar_idx);
}

static void virtio_pci_modern_mem_region_unmap(VirtIOPCIProxy *proxy,
                                               VirtIOPCIRegion *region)
{
    memory_region_del_subregion(&proxy->modern_bar,
                                &region->mr);
}

static void virtio_pci_modern_io_region_unmap(VirtIOPCIProxy *proxy,
                                              VirtIOPCIRegion *region)
{
    memory_region_del_subregion(&proxy->io_bar,
                                &region->mr);
}

static void virtio_pci_pre_plugged(DeviceState *d, Error **errp)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    if (gunyah_enabled()) {
        proxy->disable_legacy = ON_OFF_AUTO_ON;
        virtio_add_feature(&vdev->host_features, VIRTIO_F_IOMMU_PLATFORM);
    }

    if (virtio_pci_modern(proxy)) {
        virtio_add_feature(&vdev->host_features, VIRTIO_F_VERSION_1);
    }

    virtio_add_feature(&vdev->host_features, VIRTIO_F_BAD_FEATURE);
}


static void virtio_pci_device_plugged(DeviceState *d, Error **errp)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    VirtioBusState *bus = &proxy->bus;
    bool legacy = virtio_pci_legacy(proxy);
    bool modern;
    bool modern_pio = proxy->flags & VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY;
    uint8_t *config;
    uint32_t size;
    VirtIODevice *vdev = virtio_bus_get_device(bus);

    
    if (!proxy->ignore_backend_features &&
            !virtio_has_feature(vdev->host_features, VIRTIO_F_VERSION_1)) {
        virtio_pci_disable_modern(proxy);

        if (!legacy) {
            error_setg(errp, "Device doesn't support modern mode, and legacy"
                             " mode is disabled");
            error_append_hint(errp, "Set disable-legacy to off\n");

            return;
        }
    }

    modern = virtio_pci_modern(proxy);

    config = proxy->pci_dev.config;
    if (proxy->class_code) {
        pci_config_set_class(config, proxy->class_code);
    }

    if (legacy) {
        if (!virtio_legacy_allowed(vdev)) {
            
            if (virtio_legacy_check_disabled(vdev)) {
                warn_report("device is modern-only, but for backward "
                            "compatibility legacy is allowed");
            } else {
                error_setg(errp,
                           "device is modern-only, use disable-legacy=on");
                return;
            }
        }
        if (virtio_host_has_feature(vdev, VIRTIO_F_IOMMU_PLATFORM)) {
            error_setg(errp, "VIRTIO_F_IOMMU_PLATFORM was supported by"
                       " neither legacy nor transitional device");
            return;
        }
        
        pci_set_word(config + PCI_SUBSYSTEM_ID, virtio_bus_get_vdev_id(bus));
        if (proxy->trans_devid) {
            pci_config_set_device_id(config, proxy->trans_devid);
        }
    } else {
        
        pci_set_word(config + PCI_VENDOR_ID,
                     PCI_VENDOR_ID_REDHAT_QUMRANET);
        pci_set_word(config + PCI_DEVICE_ID,
                     PCI_DEVICE_ID_VIRTIO_10_BASE + virtio_bus_get_vdev_id(bus));
        pci_config_set_revision(config, 1);
    }
    config[PCI_INTERRUPT_PIN] = !gunyah_enabled();


    if (modern) {
        struct virtio_pci_cap cap = {
            .cap_len = sizeof cap,
        };
        struct virtio_pci_notify_cap notify = {
            .cap.cap_len = sizeof notify,
            .notify_off_multiplier =
                cpu_to_le32(virtio_pci_queue_mem_mult(proxy)),
        };
        struct virtio_pci_cfg_cap cfg = {
            .cap.cap_len = sizeof cfg,
            .cap.cfg_type = VIRTIO_PCI_CAP_PCI_CFG,
        };
        struct virtio_pci_notify_cap notify_pio = {
            .cap.cap_len = sizeof notify,
            .notify_off_multiplier = cpu_to_le32(0x0),
        };

        struct virtio_pci_cfg_cap *cfg_mask;

        virtio_pci_modern_regions_init(proxy, vdev->name);

        virtio_pci_modern_mem_region_map(proxy, &proxy->common, &cap);
        virtio_pci_modern_mem_region_map(proxy, &proxy->isr, &cap);
        virtio_pci_modern_mem_region_map(proxy, &proxy->device, &cap);
        virtio_pci_modern_mem_region_map(proxy, &proxy->notify, &notify.cap);

        if (modern_pio) {
            memory_region_init(&proxy->io_bar, OBJECT(proxy),
                               "virtio-pci-io", 0x4);
            address_space_init(&proxy->modern_cfg_io_as, &proxy->io_bar,
                               "virtio-pci-cfg-io-as");

            pci_register_bar(&proxy->pci_dev, proxy->modern_io_bar_idx,
                             PCI_BASE_ADDRESS_SPACE_IO, &proxy->io_bar);

            virtio_pci_modern_io_region_map(proxy, &proxy->notify_pio,
                                            &notify_pio.cap);
        }

        pci_register_bar(&proxy->pci_dev, proxy->modern_mem_bar_idx,
                         PCI_BASE_ADDRESS_SPACE_MEMORY |
                         PCI_BASE_ADDRESS_MEM_PREFETCH |
                         PCI_BASE_ADDRESS_MEM_TYPE_64,
                         &proxy->modern_bar);

        proxy->config_cap = virtio_pci_add_mem_cap(proxy, &cfg.cap);
        cfg_mask = (void *)(proxy->pci_dev.wmask + proxy->config_cap);
        pci_set_byte(&cfg_mask->cap.bar, ~0x0);
        pci_set_long((uint8_t *)&cfg_mask->cap.offset, ~0x0);
        pci_set_long((uint8_t *)&cfg_mask->cap.length, ~0x0);
        pci_set_long(cfg_mask->pci_cfg_data, ~0x0);
    }

    if (proxy->nvectors) {
        int err = msix_init_exclusive_bar(&proxy->pci_dev, proxy->nvectors,
                                          proxy->msix_bar_idx,
                                          gunyah_enabled() ? &error_fatal : NULL);
        if (err) {
            if (err != -ENOTSUP) {
                warn_report("unable to init msix vectors to %" PRIu32,
                            proxy->nvectors);
            }
            proxy->nvectors = 0;
        }
    }

    proxy->pci_dev.config_write = virtio_write_config;
    proxy->pci_dev.config_read = virtio_read_config;

    if (legacy) {
        size = VIRTIO_PCI_REGION_SIZE(&proxy->pci_dev)
            + virtio_bus_get_vdev_config_len(bus);
        size = pow2ceil(size);

        memory_region_init_io(&proxy->bar, OBJECT(proxy),
                              &virtio_pci_config_ops,
                              proxy, "virtio-pci", size);

        pci_register_bar(&proxy->pci_dev, proxy->legacy_io_bar_idx,
                         PCI_BASE_ADDRESS_SPACE_IO, &proxy->bar);
    }
}

static void virtio_pci_device_unplugged(DeviceState *d)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(d);
    bool modern = virtio_pci_modern(proxy);
    bool modern_pio = proxy->flags & VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY;

    virtio_pci_stop_ioeventfd(proxy);

    if (modern) {
        virtio_pci_modern_mem_region_unmap(proxy, &proxy->common);
        virtio_pci_modern_mem_region_unmap(proxy, &proxy->isr);
        virtio_pci_modern_mem_region_unmap(proxy, &proxy->device);
        virtio_pci_modern_mem_region_unmap(proxy, &proxy->notify);
        if (modern_pio) {
            virtio_pci_modern_io_region_unmap(proxy, &proxy->notify_pio);
        }
    }
}

static void virtio_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(pci_dev);
    VirtioPCIClass *k = VIRTIO_PCI_GET_CLASS(pci_dev);
    bool pcie_port = pci_bus_is_express(pci_get_bus(pci_dev)) &&
                     !pci_bus_is_root(pci_get_bus(pci_dev));

    
    proxy->legacy_io_bar_idx  = 0;
    proxy->msix_bar_idx       = 1;
    proxy->modern_io_bar_idx  = 2;
    proxy->modern_mem_bar_idx = 4;

    proxy->common.offset = 0x0;
    proxy->common.size = 0x1000;
    proxy->common.type = VIRTIO_PCI_CAP_COMMON_CFG;

    proxy->isr.offset = 0x1000;
    proxy->isr.size = 0x1000;
    proxy->isr.type = VIRTIO_PCI_CAP_ISR_CFG;

    proxy->device.offset = 0x2000;
    proxy->device.size = 0x1000;
    proxy->device.type = VIRTIO_PCI_CAP_DEVICE_CFG;

    proxy->notify.offset = 0x3000;
    proxy->notify.size = virtio_pci_queue_mem_mult(proxy) * VIRTIO_QUEUE_MAX;
    proxy->notify.type = VIRTIO_PCI_CAP_NOTIFY_CFG;

    proxy->notify_pio.offset = 0x0;
    proxy->notify_pio.size = 0x4;
    proxy->notify_pio.type = VIRTIO_PCI_CAP_NOTIFY_CFG;

    
    memory_region_init(&proxy->modern_bar, OBJECT(proxy), "virtio-pci",
                       
                       pow2ceil(proxy->notify.offset + proxy->notify.size));

    address_space_init(&proxy->modern_cfg_mem_as, &proxy->modern_bar,
                       "virtio-pci-cfg-mem-as");

    if (proxy->disable_legacy == ON_OFF_AUTO_AUTO) {
        proxy->disable_legacy = pcie_port ? ON_OFF_AUTO_ON : ON_OFF_AUTO_OFF;
    }

    if (!virtio_pci_modern(proxy) && !virtio_pci_legacy(proxy)) {
        error_setg(errp, "device cannot work as neither modern nor legacy mode"
                   " is enabled");
        error_append_hint(errp, "Set either disable-modern or disable-legacy"
                          " to off\n");
        return;
    }

    if (pcie_port && pci_is_express(pci_dev)) {
        int pos;
        uint16_t last_pcie_cap_offset = PCI_CONFIG_SPACE_SIZE;

        pos = pcie_endpoint_cap_init(pci_dev, 0);
        assert(pos > 0);

        pos = pci_pm_init(pci_dev, 0, errp);
        if (pos < 0) {
            return;
        }

        
        pci_set_word(pci_dev->config + pos + PCI_PM_PMC, 0x3);

        if (proxy->flags & VIRTIO_PCI_FLAG_AER) {
            pcie_aer_init(pci_dev, PCI_ERR_VER, last_pcie_cap_offset,
                          PCI_ERR_SIZEOF, NULL);
            last_pcie_cap_offset += PCI_ERR_SIZEOF;
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_INIT_DEVERR) {
            
            pcie_cap_deverr_init(pci_dev);
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_INIT_LNKCTL) {
            
            pcie_cap_lnkctl_init(pci_dev);
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_PM_NO_SOFT_RESET) {
            pci_set_word(pci_dev->config + pos + PCI_PM_CTRL,
                         PCI_PM_CTRL_NO_SOFT_RESET);
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_INIT_PM) {
            
            pci_set_word(pci_dev->wmask + pos + PCI_PM_CTRL,
                         PCI_PM_CTRL_STATE_MASK);
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_ATS) {
            pcie_ats_init(pci_dev, last_pcie_cap_offset,
                          proxy->flags & VIRTIO_PCI_FLAG_ATS_PAGE_ALIGNED);
            last_pcie_cap_offset += PCI_EXT_CAP_ATS_SIZEOF;
        }

        if (proxy->flags & VIRTIO_PCI_FLAG_INIT_FLR) {
            
            pcie_cap_flr_init(pci_dev);
        }
    } else {
        
        pci_dev->cap_present &= ~QEMU_PCI_CAP_EXPRESS;
    }

    virtio_pci_bus_new(&proxy->bus, sizeof(proxy->bus), proxy);
    if (k->realize) {
        k->realize(proxy, errp);
    }
}

static void virtio_pci_exit(PCIDevice *pci_dev)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(pci_dev);
    bool pcie_port = pci_bus_is_express(pci_get_bus(pci_dev)) &&
                     !pci_bus_is_root(pci_get_bus(pci_dev));
    bool modern_pio = proxy->flags & VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY;

    msix_uninit_exclusive_bar(pci_dev);
    if (proxy->flags & VIRTIO_PCI_FLAG_AER && pcie_port &&
        pci_is_express(pci_dev)) {
        pcie_aer_exit(pci_dev);
    }
    address_space_destroy(&proxy->modern_cfg_mem_as);
    if (modern_pio) {
        address_space_destroy(&proxy->modern_cfg_io_as);
    }
}

static void virtio_pci_reset(DeviceState *qdev)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(qdev);
    VirtioBusState *bus = VIRTIO_BUS(&proxy->bus);
    int i;

    virtio_bus_reset(bus);
    msix_unuse_all_vectors(&proxy->pci_dev);

    for (i = 0; i < VIRTIO_QUEUE_MAX; i++) {
        proxy->vqs[i].enabled = 0;
        proxy->vqs[i].reset = 0;
        proxy->vqs[i].num = 0;
        proxy->vqs[i].desc[0] = proxy->vqs[i].desc[1] = 0;
        proxy->vqs[i].avail[0] = proxy->vqs[i].avail[1] = 0;
        proxy->vqs[i].used[0] = proxy->vqs[i].used[1] = 0;
    }
}

static bool virtio_pci_no_soft_reset(PCIDevice *dev)
{
    uint16_t pmcsr;

    if (!pci_is_express(dev) || !(dev->cap_present & QEMU_PCI_CAP_PM)) {
        return false;
    }

    pmcsr = pci_get_word(dev->config + dev->pm_cap + PCI_PM_CTRL);

    
    return (pmcsr & PCI_PM_CTRL_NO_SOFT_RESET) &&
           (pmcsr & PCI_PM_CTRL_STATE_MASK) == 3;
}

static void virtio_pci_bus_reset_hold(Object *obj, ResetType type)
{
    PCIDevice *dev = PCI_DEVICE(obj);
    DeviceState *qdev = DEVICE(obj);

    if (virtio_pci_no_soft_reset(dev)) {
        return;
    }

    virtio_pci_reset(qdev);

    if (pci_is_express(dev)) {
        VirtIOPCIProxy *proxy = VIRTIO_PCI(dev);

        pcie_cap_deverr_reset(dev);
        pcie_cap_lnkctl_reset(dev);

        if (proxy->flags & VIRTIO_PCI_FLAG_INIT_PM) {
            pci_word_test_and_clear_mask(
                dev->config + dev->pm_cap + PCI_PM_CTRL,
                PCI_PM_CTRL_STATE_MASK);
        }
    }
}

static const Property virtio_pci_properties[] = {
    DEFINE_PROP_BIT("virtio-pci-bus-master-bug-migration", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_BUS_MASTER_BUG_MIGRATION_BIT, false),
    DEFINE_PROP_BIT("migrate-extra", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_MIGRATE_EXTRA_BIT, true),
    DEFINE_PROP_BIT("modern-pio-notify", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_MODERN_PIO_NOTIFY_BIT, false),
    DEFINE_PROP_BIT("x-disable-pcie", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_DISABLE_PCIE_BIT, false),
    DEFINE_PROP_BIT("page-per-vq", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_PAGE_PER_VQ_BIT, false),
    DEFINE_PROP_BOOL("x-ignore-backend-features", VirtIOPCIProxy,
                     ignore_backend_features, false),
    DEFINE_PROP_BIT("ats", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_ATS_BIT, false),
    DEFINE_PROP_BIT("x-ats-page-aligned", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_ATS_PAGE_ALIGNED_BIT, true),
    DEFINE_PROP_BIT("x-pcie-deverr-init", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_INIT_DEVERR_BIT, true),
    DEFINE_PROP_BIT("x-pcie-lnkctl-init", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_INIT_LNKCTL_BIT, true),
    DEFINE_PROP_BIT("x-pcie-pm-init", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_INIT_PM_BIT, true),
    DEFINE_PROP_BIT("x-pcie-pm-no-soft-reset", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_PM_NO_SOFT_RESET_BIT, false),
    DEFINE_PROP_BIT("x-pcie-flr-init", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_INIT_FLR_BIT, true),
    DEFINE_PROP_BIT("aer", VirtIOPCIProxy, flags,
                    VIRTIO_PCI_FLAG_AER_BIT, false),
};

static void virtio_pci_dc_realize(DeviceState *qdev, Error **errp)
{
    VirtioPCIClass *vpciklass = VIRTIO_PCI_GET_CLASS(qdev);
    VirtIOPCIProxy *proxy = VIRTIO_PCI(qdev);
    PCIDevice *pci_dev = &proxy->pci_dev;

    if (!(proxy->flags & VIRTIO_PCI_FLAG_DISABLE_PCIE) &&
        virtio_pci_modern(proxy)) {
        pci_dev->cap_present |= QEMU_PCI_CAP_EXPRESS;
    }

    vpciklass->parent_dc_realize(qdev, errp);
}

static int virtio_pci_sync_config(DeviceState *dev, Error **errp)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(dev);
    VirtIODevice *vdev = virtio_bus_get_device(&proxy->bus);

    return qdev_sync_config(DEVICE(vdev), errp);
}

static void virtio_pci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    VirtioPCIClass *vpciklass = VIRTIO_PCI_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    device_class_set_props(dc, virtio_pci_properties);
    k->realize = virtio_pci_realize;
    k->exit = virtio_pci_exit;
    k->vendor_id = PCI_VENDOR_ID_REDHAT_QUMRANET;
    k->revision = VIRTIO_PCI_ABI_VERSION;
    k->class_id = PCI_CLASS_OTHERS;
    device_class_set_parent_realize(dc, virtio_pci_dc_realize,
                                    &vpciklass->parent_dc_realize);
    rc->phases.hold = virtio_pci_bus_reset_hold;
    dc->sync_config = virtio_pci_sync_config;
}

static const TypeInfo virtio_pci_info = {
    .name          = TYPE_VIRTIO_PCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(VirtIOPCIProxy),
    .class_init    = virtio_pci_class_init,
    .class_size    = sizeof(VirtioPCIClass),
    .abstract      = true,
};

static const Property virtio_pci_generic_properties[] = {
    DEFINE_PROP_ON_OFF_AUTO("disable-legacy", VirtIOPCIProxy, disable_legacy,
                            ON_OFF_AUTO_AUTO),
    DEFINE_PROP_BOOL("disable-modern", VirtIOPCIProxy, disable_modern, false),
};

static void virtio_pci_base_class_init(ObjectClass *klass, void *data)
{
    const VirtioPCIDeviceTypeInfo *t = data;
    if (t->class_init) {
        t->class_init(klass, NULL);
    }
}

static void virtio_pci_generic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, virtio_pci_generic_properties);
}

static void virtio_pci_transitional_instance_init(Object *obj)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(obj);

    proxy->disable_legacy = ON_OFF_AUTO_OFF;
    proxy->disable_modern = false;
}

static void virtio_pci_non_transitional_instance_init(Object *obj)
{
    VirtIOPCIProxy *proxy = VIRTIO_PCI(obj);

    proxy->disable_legacy = ON_OFF_AUTO_ON;
    proxy->disable_modern = false;
}

void virtio_pci_types_register(const VirtioPCIDeviceTypeInfo *t)
{
    char *base_name = NULL;
    TypeInfo base_type_info = {
        .name          = t->base_name,
        .parent        = t->parent ? t->parent : TYPE_VIRTIO_PCI,
        .instance_size = t->instance_size,
        .instance_init = t->instance_init,
        .instance_finalize = t->instance_finalize,
        .class_size    = t->class_size,
        .abstract      = true,
        .interfaces    = t->interfaces,
    };
    TypeInfo generic_type_info = {
        .name = t->generic_name,
        .parent = base_type_info.name,
        .class_init = virtio_pci_generic_class_init,
        .interfaces = (InterfaceInfo[]) {
            { INTERFACE_PCIE_DEVICE },
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { }
        },
    };

    if (!base_type_info.name) {
        
        
        base_name = g_strdup_printf("%s-base-type", t->generic_name);
        base_type_info.name = base_name;
        base_type_info.class_init = virtio_pci_generic_class_init;

        generic_type_info.parent = base_name;
        generic_type_info.class_init = virtio_pci_base_class_init;
        generic_type_info.class_data = (void *)t;

        assert(!t->non_transitional_name);
        assert(!t->transitional_name);
    } else {
        base_type_info.class_init = virtio_pci_base_class_init;
        base_type_info.class_data = (void *)t;
    }

    type_register_static(&base_type_info);
    if (generic_type_info.name) {
        type_register_static(&generic_type_info);
    }

    if (t->non_transitional_name) {
        const TypeInfo non_transitional_type_info = {
            .name          = t->non_transitional_name,
            .parent        = base_type_info.name,
            .instance_init = virtio_pci_non_transitional_instance_init,
            .interfaces = (InterfaceInfo[]) {
                { INTERFACE_PCIE_DEVICE },
                { INTERFACE_CONVENTIONAL_PCI_DEVICE },
                { }
            },
        };
        type_register_static(&non_transitional_type_info);
    }

    if (t->transitional_name) {
        const TypeInfo transitional_type_info = {
            .name          = t->transitional_name,
            .parent        = base_type_info.name,
            .instance_init = virtio_pci_transitional_instance_init,
            .interfaces = (InterfaceInfo[]) {
                
                { INTERFACE_CONVENTIONAL_PCI_DEVICE },
                { }
            },
        };
        type_register_static(&transitional_type_info);
    }
    g_free(base_name);
}

unsigned virtio_pci_optimal_num_queues(unsigned fixed_queues)
{
    
    unsigned num_queues = current_machine->smp.cpus;

    
    num_queues = MIN(num_queues, PCI_MSIX_FLAGS_QSIZE - fixed_queues);

    
    return MIN(num_queues, VIRTIO_QUEUE_MAX - fixed_queues);
}



static void virtio_pci_bus_new(VirtioBusState *bus, size_t bus_size,
                               VirtIOPCIProxy *dev)
{
    DeviceState *qdev = DEVICE(dev);
    char virtio_bus_name[] = "virtio-bus";

    qbus_init(bus, bus_size, TYPE_VIRTIO_PCI_BUS, qdev, virtio_bus_name);
}

static void virtio_pci_bus_class_init(ObjectClass *klass, void *data)
{
    BusClass *bus_class = BUS_CLASS(klass);
    VirtioBusClass *k = VIRTIO_BUS_CLASS(klass);
    bus_class->max_dev = 1;
    k->notify = virtio_pci_notify;
    k->save_config = virtio_pci_save_config;
    k->load_config = virtio_pci_load_config;
    k->save_queue = virtio_pci_save_queue;
    k->load_queue = virtio_pci_load_queue;
    k->save_extra_state = virtio_pci_save_extra_state;
    k->load_extra_state = virtio_pci_load_extra_state;
    k->has_extra_state = virtio_pci_has_extra_state;
    k->query_guest_notifiers = virtio_pci_query_guest_notifiers;
    k->set_guest_notifiers = virtio_pci_set_guest_notifiers;
    k->set_host_notifier_mr = virtio_pci_set_host_notifier_mr;
    k->vmstate_change = virtio_pci_vmstate_change;
    k->pre_plugged = virtio_pci_pre_plugged;
    k->device_plugged = virtio_pci_device_plugged;
    k->device_unplugged = virtio_pci_device_unplugged;
    k->query_nvectors = virtio_pci_query_nvectors;
    k->ioeventfd_enabled = virtio_pci_ioeventfd_enabled;
    k->ioeventfd_assign = virtio_pci_ioeventfd_assign;
    k->get_dma_as = virtio_pci_get_dma_as;
    k->iommu_enabled = virtio_pci_iommu_enabled;
    k->queue_enabled = virtio_pci_queue_enabled;
}

static const TypeInfo virtio_pci_bus_info = {
    .name          = TYPE_VIRTIO_PCI_BUS,
    .parent        = TYPE_VIRTIO_BUS,
    .instance_size = sizeof(VirtioPCIBusState),
    .class_size    = sizeof(VirtioPCIBusClass),
    .class_init    = virtio_pci_bus_class_init,
};

static void virtio_pci_register_types(void)
{
    
    type_register_static(&virtio_pci_bus_info);
    type_register_static(&virtio_pci_info);
}

type_init(virtio_pci_register_types)
