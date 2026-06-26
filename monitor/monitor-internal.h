
#ifndef MONITOR_INTERNAL_H
#define MONITOR_INTERNAL_H

#include "chardev/char-fe.h"
#include "monitor/monitor.h"
#include "qapi/qapi-types-control.h"
#include "qapi/qmp-registry.h"
#include "qobject/json-parser.h"
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
    bool is_qmp;
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

typedef struct {
    Monitor common;
    JSONMessageParser parser;
    bool pretty;
    const QmpCommandList *commands;
    bool capab_offered[QMP_CAPABILITY__MAX]; /* capabilities offered */
    bool capab[QMP_CAPABILITY__MAX];         /* offered and accepted */
    QemuMutex qmp_queue_lock;
    GQueue *qmp_requests;
} MonitorQMP;

static inline bool monitor_is_qmp(const Monitor *mon)
{
    return mon->is_qmp;
}

typedef QTAILQ_HEAD(MonitorList, Monitor) MonitorList;
extern IOThread *mon_iothread;
extern Coroutine *qmp_dispatcher_co;
extern bool qmp_dispatcher_co_shutdown;
extern QmpCommandList qmp_commands, qmp_cap_negotiation_commands;
extern QemuMutex monitor_lock;
extern MonitorList mon_list;

extern HMPCommand hmp_cmds[];

void monitor_data_init(Monitor *mon, bool is_qmp, bool skip_flush,
                       bool use_io_thread);
void monitor_data_destroy(Monitor *mon);
int monitor_can_read(void *opaque);
void monitor_list_append(Monitor *mon);
void monitor_fdsets_cleanup(void);

void qmp_send_response(MonitorQMP *mon, const QDict *rsp);
void monitor_data_destroy_qmp(MonitorQMP *mon);
void coroutine_fn monitor_qmp_dispatcher_co(void *data);
void qmp_dispatcher_co_wake(void);

int get_monitor_def(Monitor *mon, int64_t *pval, const char *name);
void handle_hmp_command(MonitorHMP *mon, const char *cmdline);
int hmp_compare_cmd(const char *name, const char *list);

#endif
