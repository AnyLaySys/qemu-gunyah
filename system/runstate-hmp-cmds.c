#include "qemu/osdep.h"
#include "monitor/hmp.h"
#include "monitor/monitor.h"
#include "qapi/qapi-commands-run-state.h"
#include "qobject/qdict.h"

void hmp_info_status(Monitor *mon, const QDict *qdict)
{
    StatusInfo *info = qmp_query_status(NULL);
    monitor_printf(mon, "VM status: %s", info->running ? "running" : "paused");
    if (!info->running && info->status != RUN_STATE_PAUSED) {
        monitor_printf(mon, " (%s)", RunState_str(info->status));
    }
    monitor_printf(mon, "\n");
    qapi_free_StatusInfo(info);
}
