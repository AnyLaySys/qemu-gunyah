
#include "qemu/osdep.h"
#include "hw/pci/pcie_port.h"
#include "hw/qdev-properties.h"
#include "qemu/module.h"
#include "hw/hotplug.h"

void pcie_port_init_reg(PCIDevice *d)
{
    pci_set_word(d->config + PCI_STATUS, 0);
    pci_set_word(d->config + PCI_SEC_STATUS, 0);

    pci_word_test_and_clear_mask(d->wmask + PCI_BRIDGE_CONTROL,
                                 PCI_BRIDGE_CTL_MASTER_ABORT |
                                 PCI_BRIDGE_CTL_FAST_BACK |
                                 PCI_BRIDGE_CTL_DISCARD |
                                 PCI_BRIDGE_CTL_SEC_DISCARD |
                                 PCI_BRIDGE_CTL_DISCARD_STATUS |
                                 PCI_BRIDGE_CTL_DISCARD_SERR);
}

struct PCIEChassis {
    uint8_t     number;

    QLIST_HEAD(, PCIESlot) slots;
    QLIST_ENTRY(PCIEChassis) next;
};

static QLIST_HEAD(, PCIEChassis) chassis = QLIST_HEAD_INITIALIZER(chassis);

static struct PCIEChassis *pcie_chassis_find(uint8_t chassis_number)
{
    struct PCIEChassis *c;
    QLIST_FOREACH(c, &chassis, next) {
        if (c->number == chassis_number) {
            break;
        }
    }
    return c;
}

void pcie_chassis_create(uint8_t chassis_number)
{
    struct PCIEChassis *c;
    c = pcie_chassis_find(chassis_number);
    if (c) {
        return;
    }
    c = g_malloc0(sizeof(*c));
    c->number = chassis_number;
    QLIST_INIT(&c->slots);
    QLIST_INSERT_HEAD(&chassis, c, next);
}

static PCIESlot *pcie_chassis_find_slot_with_chassis(struct PCIEChassis *c,
                                                     uint8_t slot)
{
    PCIESlot *s;
    QLIST_FOREACH(s, &c->slots, next) {
        if (s->slot == slot) {
            break;
        }
    }
    return s;
}

int pcie_chassis_add_slot(struct PCIESlot *slot)
{
    struct PCIEChassis *c;
    c = pcie_chassis_find(slot->chassis);
    if (!c) {
        return -ENODEV;
    }
    if (pcie_chassis_find_slot_with_chassis(c, slot->slot)) {
        return -EBUSY;
    }
    QLIST_INSERT_HEAD(&c->slots, slot, next);
    return 0;
}

void pcie_chassis_del_slot(PCIESlot *s)
{
    QLIST_REMOVE(s, next);
}

static const Property pcie_port_props[] = {
    DEFINE_PROP_UINT8("port", PCIEPort, port, 0),
    DEFINE_PROP_UINT16("aer_log_max", PCIEPort,
                       parent_obj.parent_obj.exp.aer_log.log_max,
                       PCIE_AER_LOG_MAX_DEFAULT),
};

static void pcie_port_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_props(dc, pcie_port_props);
}

PCIDevice *pcie_find_port_by_pn(PCIBus *bus, uint8_t pn)
{
    int devfn;

    for (devfn = 0; devfn < ARRAY_SIZE(bus->devices); devfn++) {
        PCIDevice *d = bus->devices[devfn];
        PCIEPort *port;

        if (!d || !pci_is_express(d) || !d->exp.exp_cap) {
            continue;
        }

        if (!object_dynamic_cast(OBJECT(d), TYPE_PCIE_PORT)) {
            continue;
        }

        port = PCIE_PORT(d);
        if (port->port == pn) {
            return d;
        }
    }

    return NULL;
}

PCIDevice *pcie_find_port_first(PCIBus *bus)
{
    int devfn;

    for (devfn = 0; devfn < ARRAY_SIZE(bus->devices); devfn++) {
        PCIDevice *d = bus->devices[devfn];

        if (!d || !pci_is_express(d) || !d->exp.exp_cap) {
            continue;
        }

        if (object_dynamic_cast(OBJECT(d), TYPE_PCIE_PORT)) {
            return d;
        }
    }

    return NULL;
}

int pcie_count_ds_ports(PCIBus *bus)
{
    int dsp_count = 0;
    int devfn;

    for (devfn = 0; devfn < ARRAY_SIZE(bus->devices); devfn++) {
        PCIDevice *d = bus->devices[devfn];

        if (!d || !pci_is_express(d) || !d->exp.exp_cap) {
            continue;
        }
        if (object_dynamic_cast(OBJECT(d), TYPE_PCIE_PORT)) {
            dsp_count++;
        }
    }
    return dsp_count;
}

static bool pcie_slot_is_hotpluggbale_bus(HotplugHandler *plug_handler,
                                          BusState *bus)
{
    PCIESlot *s = PCIE_SLOT(bus->parent);
    return s->hotplug;
}

static const TypeInfo pcie_port_type_info = {
    .name = TYPE_PCIE_PORT,
    .parent = TYPE_PCI_BRIDGE,
    .instance_size = sizeof(PCIEPort),
    .abstract = true,
    .class_init = pcie_port_class_init,
};

static const Property pcie_slot_props[] = {
    DEFINE_PROP_UINT8("chassis", PCIESlot, chassis, 0),
    DEFINE_PROP_UINT16("slot", PCIESlot, slot, 0),
    DEFINE_PROP_BOOL("hotplug", PCIESlot, hotplug, true),
    DEFINE_PROP_BOOL("x-do-not-expose-native-hotplug-cap", PCIESlot,
                     hide_native_hotplug_cap, false),
};

static void pcie_slot_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(oc);

    device_class_set_props(dc, pcie_slot_props);
    hc->pre_plug = pcie_cap_slot_pre_plug_cb;
    hc->plug = pcie_cap_slot_plug_cb;
    hc->unplug = pcie_cap_slot_unplug_cb;
    hc->unplug_request = pcie_cap_slot_unplug_request_cb;
    hc->is_hotpluggable_bus = pcie_slot_is_hotpluggbale_bus;
}

static const TypeInfo pcie_slot_type_info = {
    .name = TYPE_PCIE_SLOT,
    .parent = TYPE_PCIE_PORT,
    .instance_size = sizeof(PCIESlot),
    .abstract = true,
    .class_init = pcie_slot_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_HOTPLUG_HANDLER },
        { }
    }
};

static void pcie_port_register_types(void)
{
    type_register_static(&pcie_port_type_info);
    type_register_static(&pcie_slot_type_info);
}

type_init(pcie_port_register_types)
