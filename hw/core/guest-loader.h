
#ifndef GUEST_LOADER_H
#define GUEST_LOADER_H

#include "hw/qdev-core.h"
#include "qom/object.h"

struct GuestLoaderState {
    DeviceState parent_obj;

    uint64_t addr;
    char *kernel;
    char *args;
    char *initrd;
};

#define TYPE_GUEST_LOADER "guest-loader"
OBJECT_DECLARE_SIMPLE_TYPE(GuestLoaderState, GUEST_LOADER)

#endif
