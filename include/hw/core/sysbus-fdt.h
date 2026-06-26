
#ifndef HW_ARM_SYSBUS_FDT_H
#define HW_ARM_SYSBUS_FDT_H

#include "exec/hwaddr.h"

void platform_bus_add_all_fdt_nodes(void *fdt, const char *intc, hwaddr addr,
                                    hwaddr bus_size, int irq_start);
#endif
