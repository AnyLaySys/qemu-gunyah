
#include "qemu/osdep.h"
#include "system/system.h"
#include "system/runstate.h"
#include "system/runstate-action.h"
#include "system/block-backend.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-control.h"
#include "qapi/qapi-commands-misc.h"

NameInfo *qmp_query_name(Error **errp)
{
    NameInfo *info = g_malloc0(sizeof(*info));

    info->name = g_strdup(qemu_name);
    return info;
}

void qmp_quit(Error **errp)
{
    shutdown_action = SHUTDOWN_ACTION_POWEROFF;
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_QMP_QUIT);
}

void qmp_stop(Error **errp)
{
    vm_stop(RUN_STATE_PAUSED);
}

void qmp_cont(Error **errp)
{
    BlockBackend *blk;

    if (runstate_needs_reset()) {
        error_setg(errp, "Resetting the Virtual Machine is required");
        return;
    } else if (runstate_check(RUN_STATE_SUSPENDED)) {
        return;
    }

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        blk_iostatus_reset(blk);
    }

    vm_start();
}
