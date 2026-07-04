

#ifndef HW_ARM_GIC_H
#define HW_ARM_GIC_H

#include "arm_gic_common.h"
#include "qom/object.h"

#define GIC_TARGETLIST_BITS 8
#define GIC_MAX_PRIORITY_BITS 8
#define GIC_MIN_PRIORITY_BITS 4

#define TYPE_ARM_GIC "arm_gic"
typedef struct ARMGICClass ARMGICClass;
DECLARE_OBJ_CHECKERS(GICState, ARMGICClass,
                     ARM_GIC, TYPE_ARM_GIC)

struct ARMGICClass {
    ARMGICCommonClass parent_class;

    DeviceRealize parent_realize;
};

static inline const char *gic_class_name(void)
{
    return "arm_gic";
}

#endif
