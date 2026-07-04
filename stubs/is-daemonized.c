#include "qemu/osdep.h"

#ifndef _WIN32
bool is_daemonized(void)
{
    return false;
}
#endif
