
#ifndef HW_ARM_GICV3_H
#define HW_ARM_GICV3_H

#include "arm_gicv3_common.h"
#include "qom/object.h"

#define TYPE_ARM_GICV3 "arm-gicv3"
typedef struct ARMGICv3Class ARMGICv3Class;
DECLARE_OBJ_CHECKERS(GICv3State, ARMGICv3Class,
                     ARM_GICV3, TYPE_ARM_GICV3)

struct ARMGICv3Class {
    ARMGICv3CommonClass parent_class;

    DeviceRealize parent_realize;
};

#endif
