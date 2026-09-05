
#include "qemu/osdep.h"
#include "system/system.h"
#include "system/runstate.h"
#include "system/runstate-action.h"
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
