
#ifndef MONITOR_INTERNAL_H
#define MONITOR_INTERNAL_H

#include "chardev/char-fe.h"
#include "monitor/monitor.h"
#include "qapi/qapi-types-control.h"
#include "system/iothread.h"

struct Monitor {
    CharBackend chr;
    int suspend_cnt;            /* Needs to be accessed atomically */
    bool skip_flush;
    bool use_io_thread;

    char *mon_cpu_path;
    QTAILQ_ENTRY(Monitor) entry;

    QemuMutex mon_lock;

    GString *outbuf;
    GString *inbuf;
    guint out_watch;
    int mux_out;
    int reset_seen;
};

typedef QTAILQ_HEAD(MonitorList, Monitor) MonitorList;
extern IOThread *mon_iothread;
extern QemuMutex monitor_lock;
extern MonitorList mon_list;

void monitor_data_init(Monitor *mon, bool skip_flush, bool use_io_thread);
void monitor_data_destroy(Monitor *mon);
int monitor_can_read(void *opaque);
void monitor_list_append(Monitor *mon);

#endif
