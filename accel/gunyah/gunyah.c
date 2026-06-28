#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include "qemu/osdep.h"
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
#include "qemu/typedefs.h"
#include "qemu/units.h"
#include "hw/core/cpu.h"
#include "system/cpus.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "linux-headers/linux/gunyah.h"
#include "exec/memory.h"
#include "qemu/error-report.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "qapi/error.h"
#include "qemu/event_notifier.h"
#include "qemu/main-loop.h"
#include "system/runstate.h"
#include "qemu/guest-random.h"
#include "gunyah-signal.c"
#include "gunyah-ioctl.c"
#include "gunyah-mem.c"
#include "gunyah-irq.c"
#include "gunyah-vm-start.c"
#include "gunyah-mmio.c"
#include "gunyah-page-fault.c"
#include "gunyah-vcpu.c"
