#include "qemu/osdep.h"
#include "monitor/hmp.h"
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-control.h"
#include "qapi/qapi-commands-misc.h"
#include "qobject/qdict.h"

bool hmp_handle_error(Monitor *mon, Error *err)
{
    if (err) {
        error_reportf_err(err, "Error: ");
        return true;
    }
    return false;
}

void hmp_info_name(Monitor *mon, const QDict *qdict)
{
    NameInfo *info = qmp_query_name(NULL);
    if (info->name) {
        monitor_printf(mon, "%s\n", info->name);
    }
    qapi_free_NameInfo(info);
}

void hmp_info_version(Monitor *mon, const QDict *qdict)
{
    VersionInfo *info = qmp_query_version(NULL);
    monitor_printf(mon, "%" PRId64 ".%" PRId64 ".%" PRId64 "%s\n",
                   info->qemu->major, info->qemu->minor, info->qemu->micro,
                   info->package);
    qapi_free_VersionInfo(info);
}

void hmp_quit(Monitor *mon, const QDict *qdict)
{
    monitor_suspend(mon);
    qmp_quit(NULL);
}

void hmp_help(Monitor *mon, const QDict *qdict)
{
    hmp_help_cmd(mon, qdict_get_try_str(qdict, "name"));
}

void hmp_info_help(Monitor *mon, const QDict *qdict)
{
    hmp_help_cmd(mon, "info");
}
