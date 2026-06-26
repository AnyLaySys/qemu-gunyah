
#ifndef QEMU_RESERVED_REGION_H
#define QEMU_RESERVED_REGION_H

#include "exec/memory.h"

GList *resv_region_list_insert(GList *list, ReservedRegion *reg);

#endif
