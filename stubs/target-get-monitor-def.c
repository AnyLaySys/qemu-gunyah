
#include "qemu/osdep.h"

int target_get_monitor_def(CPUState *cs, const char *name, uint64_t *pval);

int target_get_monitor_def(CPUState *cs, const char *name, uint64_t *pval)
{
    return -1;
}
