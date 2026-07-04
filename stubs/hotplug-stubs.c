#include "qemu/osdep.h"
#include "hw/qdev-core.h"

HotplugHandler *qdev_get_hotplug_handler(DeviceState *dev)
{
    return NULL;
}

void hotplug_handler_pre_plug(HotplugHandler *plug_handler,
                              DeviceState *plugged_dev,
                              Error **errp)
{
    g_assert_not_reached();
}

void hotplug_handler_plug(HotplugHandler *plug_handler,
                          DeviceState *plugged_dev,
                          Error **errp)
{
    g_assert_not_reached();
}
