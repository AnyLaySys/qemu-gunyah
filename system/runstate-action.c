
#include "qemu/osdep.h"
#include "system/runstate-action.h"
#include "qemu/config-file.h"
#include "qapi/error.h"
#include "qemu/option_int.h"

RebootAction reboot_action = REBOOT_ACTION_RESET;
ShutdownAction shutdown_action = SHUTDOWN_ACTION_POWEROFF;
PanicAction panic_action = PANIC_ACTION_SHUTDOWN;

void qmp_set_action(bool has_reboot, RebootAction reboot,
                    bool has_shutdown, ShutdownAction shutdown,
                    bool has_panic, PanicAction panic,
                    Error **errp)
{
    if (has_reboot) {
        reboot_action = reboot;
    }

    if (has_panic) {
        panic_action = panic;
    }

    if (has_shutdown) {
        shutdown_action = shutdown;
    }
}
