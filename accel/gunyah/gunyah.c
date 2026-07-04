#include "qemu/osdep.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <ucontext.h>
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 1
#endif
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif
#ifndef MADV_POPULATE_WRITE
#define MADV_POPULATE_WRITE 23
#endif
#ifndef MADV_COLLAPSE
#define MADV_COLLAPSE 25
#endif
#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "gunyah-ioctl.c"
#include "gunyah-mem.c"
#include "gunyah-mmio.c"
#include "gunyah-page-fault.c"
#include "gunyah-signal.c"
#include "gunyah-vcpu.c"
#include "gunyah-vm-start.c"
#include "hw/boards.h"
#include "hw/core/cpu.h"
#include "linux-headers/linux/gunyah.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/event_notifier.h"
#include "qemu/guest-random.h"
#include "qemu/main-loop.h"
#include "qemu/typedefs.h"
#include "qemu/units.h"
#include "system/cpus.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "system/runstate.h"