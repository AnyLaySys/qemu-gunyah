
#ifndef HW_ARM_LINUX_BOOT_IF_H
#define HW_ARM_LINUX_BOOT_IF_H

#include "qom/object.h"

#define TYPE_ARM_LINUX_BOOT_IF "arm-linux-boot-if"
typedef struct ARMLinuxBootIfClass ARMLinuxBootIfClass;
DECLARE_CLASS_CHECKERS(ARMLinuxBootIfClass, ARM_LINUX_BOOT_IF,
                       TYPE_ARM_LINUX_BOOT_IF)
#define ARM_LINUX_BOOT_IF(obj) \
    INTERFACE_CHECK(ARMLinuxBootIf, (obj), TYPE_ARM_LINUX_BOOT_IF)

typedef struct ARMLinuxBootIf ARMLinuxBootIf;

struct ARMLinuxBootIfClass {
    InterfaceClass parent_class;

    void (*arm_linux_init)(ARMLinuxBootIf *obj, bool secure_boot);
};

#endif
