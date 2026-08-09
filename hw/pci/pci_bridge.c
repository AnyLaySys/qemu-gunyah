
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci_bridge.h"
#include "hw/pci/pci_bus.h"
#include "qemu/module.h"
#include "qemu/range.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"

#define PCI_SSVID_SIZEOF        8
#define PCI_SSVID_SVID          4
#define PCI_SSVID_SSID          6

int pci_bridge_ssvid_init(PCIDevice *dev, uint8_t offset,
                          uint16_t svid, uint16_t ssid,
                          Error **errp)
{
    int pos;

    pos = pci_add_capability(dev, PCI_CAP_ID_SSVID, offset,
                             PCI_SSVID_SIZEOF, errp);
    if (pos < 0) {
        return pos;
    }

    pci_set_word(dev->config + pos + PCI_SSVID_SVID, svid);
    pci_set_word(dev->config + pos + PCI_SSVID_SSID, ssid);
    return pos;
}

PCIDevice *pci_bridge_get_device(PCIBus *bus)
{
    return bus->parent_dev;
}

PCIBus *pci_bridge_get_sec_bus(PCIBridge *br)
{
    return &br->sec_bus;
}

static uint32_t pci_config_get_io_base(const PCIDevice *d,
                                       uint32_t base, uint32_t base_upper16)
{
    uint32_t val;

    val = ((uint32_t)d->config[base] & PCI_IO_RANGE_MASK) << 8;
    if (d->config[base] & PCI_IO_RANGE_TYPE_32) {
        val |= (uint32_t)pci_get_word(d->config + base_upper16) << 16;
    }
    return val;
}

static pcibus_t pci_config_get_memory_base(const PCIDevice *d, uint32_t base)
{
    return ((pcibus_t)pci_get_word(d->config + base) & PCI_MEMORY_RANGE_MASK)
        << 16;
}

static pcibus_t pci_config_get_pref_base(const PCIDevice *d,
                                         uint32_t base, uint32_t upper)
{
    pcibus_t tmp;
    pcibus_t val;

    tmp = (pcibus_t)pci_get_word(d->config + base);
    val = (tmp & PCI_PREF_RANGE_MASK) << 16;
    if (tmp & PCI_PREF_RANGE_TYPE_64) {
        val |= (pcibus_t)pci_get_long(d->config + upper) << 32;
    }
    return val;
}

pcibus_t pci_bridge_get_base(const PCIDevice *bridge, uint8_t type)
{
    pcibus_t base;
    if (type & PCI_BASE_ADDRESS_SPACE_IO) {
        base = pci_config_get_io_base(bridge,
                                      PCI_IO_BASE, PCI_IO_BASE_UPPER16);
    } else {
        if (type & PCI_BASE_ADDRESS_MEM_PREFETCH) {
            base = pci_config_get_pref_base(
                bridge, PCI_PREF_MEMORY_BASE, PCI_PREF_BASE_UPPER32);
        } else {
            base = pci_config_get_memory_base(bridge, PCI_MEMORY_BASE);
        }
    }

    return base;
}

pcibus_t pci_bridge_get_limit(const PCIDevice *bridge, uint8_t type)
{
    pcibus_t limit;
    if (type & PCI_BASE_ADDRESS_SPACE_IO) {
        limit = pci_config_get_io_base(bridge,
                                      PCI_IO_LIMIT, PCI_IO_LIMIT_UPPER16);
        limit |= 0xfff;         /* PCI bridge spec 3.2.5.6. */
    } else {
        if (type & PCI_BASE_ADDRESS_MEM_PREFETCH) {
            limit = pci_config_get_pref_base(
                bridge, PCI_PREF_MEMORY_LIMIT, PCI_PREF_LIMIT_UPPER32);
        } else {
            limit = pci_config_get_memory_base(bridge, PCI_MEMORY_LIMIT);
        }
        limit |= 0xfffff;       /* PCI bridge spec 3.2.5.{1, 8}. */
    }
    return limit;
}

static void pci_bridge_init_alias(PCIBridge *bridge, MemoryRegion *alias,
                                  uint8_t type, const char *name,
                                  MemoryRegion *space,
                                  MemoryRegion *parent_space,
                                  bool enabled)
{
    PCIDevice *bridge_dev = PCI_DEVICE(bridge);
    pcibus_t base = pci_bridge_get_base(bridge_dev, type);
    pcibus_t limit = pci_bridge_get_limit(bridge_dev, type);
    pcibus_t size = enabled && limit >= base ? limit + 1 - base : 0;

    memory_region_init_alias(alias, OBJECT(bridge), name, space, base, size);
    memory_region_add_subregion_overlap(parent_space, base, alias, 1);
}

static void pci_bridge_region_init(PCIBridge *br)
{
    PCIDevice *pd = PCI_DEVICE(br);
    PCIBus *parent = pci_get_bus(pd);
    PCIBridgeWindows *w = &br->windows;
    uint16_t cmd = pci_get_word(pd->config + PCI_COMMAND);

    pci_bridge_init_alias(br, &w->alias_pref_mem,
                          PCI_BASE_ADDRESS_MEM_PREFETCH,
                          "pci_bridge_pref_mem",
                          &br->address_space_mem,
                          parent->address_space_mem,
                          cmd & PCI_COMMAND_MEMORY);
    pci_bridge_init_alias(br, &w->alias_mem,
                          PCI_BASE_ADDRESS_SPACE_MEMORY,
                          "pci_bridge_mem",
                          &br->address_space_mem,
                          parent->address_space_mem,
                          cmd & PCI_COMMAND_MEMORY);
    pci_bridge_init_alias(br, &w->alias_io,
                          PCI_BASE_ADDRESS_SPACE_IO,
                          "pci_bridge_io",
                          &br->address_space_io,
                          parent->address_space_io,
                          cmd & PCI_COMMAND_IO);
}

static void pci_bridge_region_del(PCIBridge *br, PCIBridgeWindows *w)
{
    PCIBus *parent = pci_get_bus(PCI_DEVICE(br));

    memory_region_del_subregion(parent->address_space_io, &w->alias_io);
    memory_region_del_subregion(parent->address_space_mem, &w->alias_mem);
    memory_region_del_subregion(parent->address_space_mem, &w->alias_pref_mem);
}

static void pci_bridge_region_cleanup(PCIBridge *br, PCIBridgeWindows *w)
{
    object_unparent(OBJECT(&w->alias_io));
    object_unparent(OBJECT(&w->alias_mem));
    object_unparent(OBJECT(&w->alias_pref_mem));
}

void pci_bridge_update_mappings(PCIBridge *br)
{
    PCIBridgeWindows *w = &br->windows;

    memory_region_transaction_begin();
    pci_bridge_region_del(br, w);
    pci_bridge_region_cleanup(br, w);
    pci_bridge_region_init(br);
    memory_region_transaction_commit();
}

void pci_bridge_write_config(PCIDevice *d,
                             uint32_t address, uint32_t val, int len)
{
    PCIBridge *s = PCI_BRIDGE(d);
    uint16_t oldctl = pci_get_word(d->config + PCI_BRIDGE_CONTROL);
    uint16_t newctl;

    pci_default_write_config(d, address, val, len);

    if (ranges_overlap(address, len, PCI_COMMAND, 2) ||

        ranges_overlap(address, len, PCI_IO_BASE, 2) ||

        ranges_overlap(address, len, PCI_MEMORY_BASE, 20) ||

        ranges_overlap(address, len, PCI_BRIDGE_CONTROL, 2)) {
        pci_bridge_update_mappings(s);
    }

    newctl = pci_get_word(d->config + PCI_BRIDGE_CONTROL);
    if (~oldctl & newctl & PCI_BRIDGE_CTL_BUS_RESET) {
        bus_cold_reset(BUS(&s->sec_bus));
    }
}

void pci_bridge_disable_base_limit(PCIDevice *dev)
{
    uint8_t *conf = dev->config;

    pci_byte_test_and_set_mask(conf + PCI_IO_BASE,
                               PCI_IO_RANGE_MASK & 0xff);
    pci_byte_test_and_clear_mask(conf + PCI_IO_LIMIT,
                                 PCI_IO_RANGE_MASK & 0xff);
    pci_word_test_and_set_mask(conf + PCI_MEMORY_BASE,
                               PCI_MEMORY_RANGE_MASK & 0xffff);
    pci_word_test_and_clear_mask(conf + PCI_MEMORY_LIMIT,
                                 PCI_MEMORY_RANGE_MASK & 0xffff);
    pci_word_test_and_set_mask(conf + PCI_PREF_MEMORY_BASE,
                               PCI_PREF_RANGE_MASK & 0xffff);
    pci_word_test_and_clear_mask(conf + PCI_PREF_MEMORY_LIMIT,
                                 PCI_PREF_RANGE_MASK & 0xffff);
    pci_set_long(conf + PCI_PREF_BASE_UPPER32, 0);
    pci_set_long(conf + PCI_PREF_LIMIT_UPPER32, 0);
}

void pci_bridge_reset(DeviceState *qdev)
{
    PCIDevice *dev = PCI_DEVICE(qdev);
    uint8_t *conf = dev->config;

    conf[PCI_PRIMARY_BUS] = 0;
    conf[PCI_SECONDARY_BUS] = 0;
    conf[PCI_SUBORDINATE_BUS] = 0;
    conf[PCI_SEC_LATENCY_TIMER] = 0;

    pci_byte_test_and_clear_mask(conf + PCI_IO_BASE,
                                 PCI_IO_RANGE_MASK & 0xff);
    pci_byte_test_and_clear_mask(conf + PCI_IO_LIMIT,
                                 PCI_IO_RANGE_MASK & 0xff);
    pci_word_test_and_clear_mask(conf + PCI_MEMORY_BASE,
                                 PCI_MEMORY_RANGE_MASK & 0xffff);
    pci_word_test_and_clear_mask(conf + PCI_MEMORY_LIMIT,
                                 PCI_MEMORY_RANGE_MASK & 0xffff);
    pci_word_test_and_clear_mask(conf + PCI_PREF_MEMORY_BASE,
                                 PCI_PREF_RANGE_MASK & 0xffff);
    pci_word_test_and_clear_mask(conf + PCI_PREF_MEMORY_LIMIT,
                                 PCI_PREF_RANGE_MASK & 0xffff);
    pci_set_long(conf + PCI_PREF_BASE_UPPER32, 0);
    pci_set_long(conf + PCI_PREF_LIMIT_UPPER32, 0);

    pci_set_word(conf + PCI_BRIDGE_CONTROL, 0);
}

void pci_bridge_initfn(PCIDevice *dev, const char *typename)
{
    PCIBus *parent = pci_get_bus(dev);
    PCIBridge *br = PCI_BRIDGE(dev);
    PCIBus *sec_bus = &br->sec_bus;

    pci_word_test_and_set_mask(dev->config + PCI_STATUS,
                               PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK);


    pci_config_set_class(dev->config, PCI_CLASS_BRIDGE_PCI);
    dev->config[PCI_HEADER_TYPE] =
        (dev->config[PCI_HEADER_TYPE] & PCI_HEADER_TYPE_MULTI_FUNCTION) |
        PCI_HEADER_TYPE_BRIDGE;
    pci_set_word(dev->config + PCI_SEC_STATUS,
                 PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK);

    if (!br->bus_name && dev->qdev.id && *dev->qdev.id) {
            br->bus_name = dev->qdev.id;
    }

    qbus_init(sec_bus, sizeof(br->sec_bus), typename, DEVICE(dev),
              br->bus_name);
    sec_bus->parent_dev = dev;
    sec_bus->map_irq = br->map_irq ? br->map_irq : pci_swizzle_map_irq_fn;
    sec_bus->address_space_mem = &br->address_space_mem;
    memory_region_init(&br->address_space_mem, OBJECT(br), "pci_bridge_pci", UINT64_MAX);
    address_space_init(&br->as_mem, &br->address_space_mem,
                       "pci_bridge_pci_mem");
    sec_bus->address_space_io = &br->address_space_io;
    memory_region_init(&br->address_space_io, OBJECT(br), "pci_bridge_io",
                       4 * GiB);
    address_space_init(&br->as_io, &br->address_space_io, "pci_bridge_pci_io");
    pci_bridge_region_init(br);
    QLIST_INIT(&sec_bus->child);
    QLIST_INSERT_HEAD(&parent->child, sec_bus, sibling);

    if (pci_bus_is_express(sec_bus) && !br->pcie_writeable_slt_bug) {
        dev->wmask[PCI_SEC_LATENCY_TIMER] = 0;
    }
}

void pci_bridge_exitfn(PCIDevice *pci_dev)
{
    PCIBridge *s = PCI_BRIDGE(pci_dev);
    assert(QLIST_EMPTY(&s->sec_bus.child));
    QLIST_REMOVE(&s->sec_bus, sibling);
    address_space_destroy(&s->as_mem);
    address_space_destroy(&s->as_io);
    pci_bridge_region_del(s, &s->windows);
    pci_bridge_region_cleanup(s, &s->windows);
}

void pci_bridge_map_irq(PCIBridge *br, const char* bus_name,
                        pci_map_irq_fn map_irq)
{
    br->map_irq = map_irq;
    br->bus_name = bus_name;
}


int pci_bridge_qemu_reserve_cap_init(PCIDevice *dev, int cap_offset,
                                     PCIResReserve res_reserve, Error **errp)
{
    if (res_reserve.mem_pref_32 != (uint64_t)-1 &&
        res_reserve.mem_pref_64 != (uint64_t)-1) {
        error_setg(errp,
                   "PCI resource reserve cap: PREF32 and PREF64 conflict");
        return -EINVAL;
    }

    if (res_reserve.mem_non_pref != (uint64_t)-1 &&
        res_reserve.mem_non_pref >= 4 * GiB) {
        error_setg(errp,
                   "PCI resource reserve cap: mem-reserve must be less than 4G");
        return -EINVAL;
    }

    if (res_reserve.mem_pref_32 != (uint64_t)-1 &&
        res_reserve.mem_pref_32 >= 4 * GiB) {
        error_setg(errp,
                   "PCI resource reserve cap: pref32-reserve  must be less than 4G");
        return -EINVAL;
    }

    if (res_reserve.bus == (uint32_t)-1 &&
        res_reserve.io == (uint64_t)-1 &&
        res_reserve.mem_non_pref == (uint64_t)-1 &&
        res_reserve.mem_pref_32 == (uint64_t)-1 &&
        res_reserve.mem_pref_64 == (uint64_t)-1) {
        return 0;
    }

    size_t cap_len = sizeof(PCIBridgeQemuCap);
    PCIBridgeQemuCap cap = {
            .len = cap_len,
            .type = REDHAT_PCI_CAP_RESOURCE_RESERVE,
            .bus_res = cpu_to_le32(res_reserve.bus),
            .io = cpu_to_le64(res_reserve.io),
            .mem = cpu_to_le32(res_reserve.mem_non_pref),
            .mem_pref_32 = cpu_to_le32(res_reserve.mem_pref_32),
            .mem_pref_64 = cpu_to_le64(res_reserve.mem_pref_64)
    };

    int offset = pci_add_capability(dev, PCI_CAP_ID_VNDR,
                                    cap_offset, cap_len, errp);
    if (offset < 0) {
        return offset;
    }

    memcpy(dev->config + offset + PCI_CAP_FLAGS,
           (char *)&cap + PCI_CAP_FLAGS,
           cap_len - PCI_CAP_FLAGS);
    return 0;
}

static const Property pci_bridge_properties[] = {
    DEFINE_PROP_BOOL("x-pci-express-writeable-slt-bug", PCIBridge,
                     pcie_writeable_slt_bug, false),
};

static void pci_bridge_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    device_class_set_props(k, pci_bridge_properties);
}

static const TypeInfo pci_bridge_type_info = {
    .name = TYPE_PCI_BRIDGE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIBridge),
    .class_init = pci_bridge_class_init,
    .abstract = true,
};

static void pci_bridge_register_types(void)
{
    type_register_static(&pci_bridge_type_info);
}

type_init(pci_bridge_register_types)
