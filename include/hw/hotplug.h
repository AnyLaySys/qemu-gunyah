#ifndef HOTPLUG_H
#define HOTPLUG_H

#include "qom/object.h"

#define TYPE_HOTPLUG_HANDLER "hotplug-handler"

typedef struct HotplugHandlerClass HotplugHandlerClass;
DECLARE_CLASS_CHECKERS(HotplugHandlerClass, HOTPLUG_HANDLER,
                       TYPE_HOTPLUG_HANDLER)
#define HOTPLUG_HANDLER(obj) \
     INTERFACE_CHECK(HotplugHandler, (obj), TYPE_HOTPLUG_HANDLER)

typedef struct HotplugHandler HotplugHandler;

typedef void (*hotplug_fn)(HotplugHandler *plug_handler,
                           DeviceState *plugged_dev, Error **errp);

struct HotplugHandlerClass {
    InterfaceClass parent;

    hotplug_fn pre_plug;
    hotplug_fn plug;
    hotplug_fn unplug_request;
    hotplug_fn unplug;
    bool (*is_hotpluggable_bus)(HotplugHandler *plug_handler, BusState *bus);
};

void hotplug_handler_plug(HotplugHandler *plug_handler,
                          DeviceState *plugged_dev,
                          Error **errp);

void hotplug_handler_pre_plug(HotplugHandler *plug_handler,
                              DeviceState *plugged_dev,
                              Error **errp);

void hotplug_handler_unplug_request(HotplugHandler *plug_handler,
                                    DeviceState *plugged_dev,
                                    Error **errp);
void hotplug_handler_unplug(HotplugHandler *plug_handler,
                            DeviceState *plugged_dev,
                            Error **errp);
#endif
