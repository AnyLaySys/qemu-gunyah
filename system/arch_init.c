#include "qemu/osdep.h"
#include "system/arch_init.h"

bool qemu_arch_available(unsigned qemu_arch_mask)
{
    return qemu_arch_mask & QEMU_ARCH;
}
