
#ifndef NMI_H
#define NMI_H

#include "qom/object.h"

#define TYPE_NMI "nmi"

typedef struct NMIClass NMIClass;
DECLARE_CLASS_CHECKERS(NMIClass, NMI,
                       TYPE_NMI)
#define NMI(obj) \
     INTERFACE_CHECK(NMIState, (obj), TYPE_NMI)

typedef struct NMIState NMIState;

struct NMIClass {
    InterfaceClass parent_class;

    void (*nmi_monitor_handler)(NMIState *n, int cpu_index, Error **errp);
};

void nmi_monitor_handle(int cpu_index, Error **errp);

#endif /* NMI_H */
