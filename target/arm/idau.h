
#ifndef TARGET_ARM_IDAU_H
#define TARGET_ARM_IDAU_H

#include "qom/object.h"

#define TYPE_IDAU_INTERFACE "idau-interface"
#define IDAU_INTERFACE(obj) \
    INTERFACE_CHECK(IDAUInterface, (obj), TYPE_IDAU_INTERFACE)
typedef struct IDAUInterfaceClass IDAUInterfaceClass;
DECLARE_CLASS_CHECKERS(IDAUInterfaceClass, IDAU_INTERFACE,
                       TYPE_IDAU_INTERFACE)

typedef struct IDAUInterface IDAUInterface;

#define IREGION_NOTVALID -1

struct IDAUInterfaceClass {
    InterfaceClass parent;

    void (*check)(IDAUInterface *ii, uint32_t address, int *iregion,
                  bool *exempt, bool *ns, bool *nsc);
};

#endif
