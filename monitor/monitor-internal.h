
#ifndef MONITOR_INTERNAL_H
#define MONITOR_INTERNAL_H

#include "chardev/char-fe.h"
#include "monitor/monitor.h"
#include "qapi/qapi-types-control.h"
#include "qemu/readline.h"
#include "system/iothread.h"


typedef struct HMPCommand {
    const char *name;
    const char *args_type;
    const char *params;
    const char *help;
    const char *flags; /* p=preconfig */
    void (*cmd)(Monitor *mon, const QDict *qdict);
    HumanReadableText *(*cmd_info_hrt)(Error **errp);
    bool coroutine;
    struct HMPCommand *sub_table;
    void (*command_completion)(ReadLineState *rs, int nb_args, const char *str);
} HMPCommand;

struct Monitor {
    CharBackend chr;
    int suspend_cnt;            /* Needs to be accessed atomically */
    bool skip_flush;
    bool use_io_thread;

    char *mon_cpu_path;
    QTAILQ_ENTRY(Monitor) entry;

    QemuMutex mon_lock;

    QLIST_HEAD(, mon_fd_t) fds;
    GString *outbuf;
    guint out_watch;
    int mux_out;
    int reset_seen;
};

struct MonitorHMP {
    Monitor common;
    bool use_readline;
    ReadLineState *rs;
};

typedef QTAILQ_HEAD(MonitorList, Monitor) MonitorList;
extern IOThread *mon_iothread;
extern QemuMutex monitor_lock;
extern MonitorList mon_list;

extern HMPCommand hmp_cmds[];

void monitor_data_init(Monitor *mon, bool skip_flush, bool use_io_thread);
void monitor_data_destroy(Monitor *mon);
int monitor_can_read(void *opaque);
void monitor_list_append(Monitor *mon);
void monitor_fdsets_cleanup(void);

int get_monitor_def(Monitor *mon, int64_t *pval, const char *name);
void handle_hmp_command(MonitorHMP *mon, const char *cmdline);
int hmp_compare_cmd(const char *name, const char *list);

#endif
