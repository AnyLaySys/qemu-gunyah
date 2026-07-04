
#ifndef FW_PATH_PROVIDER_H
#define FW_PATH_PROVIDER_H

#include "qom/object.h"

#define TYPE_FW_PATH_PROVIDER "fw-path-provider"

typedef struct FWPathProviderClass FWPathProviderClass;
DECLARE_CLASS_CHECKERS(FWPathProviderClass, FW_PATH_PROVIDER,
                       TYPE_FW_PATH_PROVIDER)
#define FW_PATH_PROVIDER(obj) \
     INTERFACE_CHECK(FWPathProvider, (obj), TYPE_FW_PATH_PROVIDER)

typedef struct FWPathProvider FWPathProvider;

struct FWPathProviderClass {
    InterfaceClass parent_class;

    char *(*get_dev_path)(FWPathProvider *p, BusState *bus, DeviceState *dev);
};

char *fw_path_provider_get_dev_path(FWPathProvider *p, BusState *bus,
                                    DeviceState *dev);
char *fw_path_provider_try_get_dev_path(Object *o, BusState *bus,
                                        DeviceState *dev);

#endif /* FW_PATH_PROVIDER_H */
