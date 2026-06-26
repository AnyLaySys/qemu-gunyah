
#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "system/runstate.h"

static int qdev_get_dev_tree_depth(DeviceState *dev)
{
    int depth;

    for (depth = 0; dev; depth++) {
        BusState *bus = dev->parent_bus;

        if (!bus) {
            break;
        }

        dev = bus->parent;
    }

    return depth;
}

VMChangeStateEntry *qdev_add_vm_change_state_handler(DeviceState *dev,
                                                     VMChangeStateHandler *cb,
                                                     void *opaque)
{
    return qdev_add_vm_change_state_handler_full(dev, cb, NULL, opaque);
}

VMChangeStateEntry *qdev_add_vm_change_state_handler_full(
    DeviceState *dev, VMChangeStateHandler *cb,
    VMChangeStateHandler *prepare_cb, void *opaque)
{
    int depth = qdev_get_dev_tree_depth(dev);

    return qemu_add_vm_change_state_handler_prio_full(cb, prepare_cb, opaque,
                                                      depth);
}
