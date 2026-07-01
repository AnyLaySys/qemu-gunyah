#ifndef MONITOR_H
#define MONITOR_H

#include "block/block.h"
#include "exec/hwaddr.h"

typedef struct MonitorOptions MonitorOptions;

extern QemuOptsList qemu_mon_opts;

Monitor *monitor_cur(void);
Monitor *monitor_set_cur(Coroutine *co, Monitor *mon);
bool monitor_cur_is_qmp(void);

void monitor_init_globals(void);
void monitor_init_globals_core(void);
int monitor_init(MonitorOptions *opts, Error **errp);
int monitor_init_opts(QemuOpts *opts, Error **errp);
void monitor_init_hmp(Chardev *chr, bool use_readline, Error **errp);
void monitor_cleanup(void);

int monitor_suspend(Monitor *mon);
void monitor_resume(Monitor *mon);

int monitor_puts(Monitor *mon, const char *str);
int monitor_vprintf(Monitor *mon, const char *fmt, va_list ap)
    G_GNUC_PRINTF(2, 0);
int monitor_printf(Monitor *mon, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
void monitor_printc(Monitor *mon, int ch);
void monitor_flush(Monitor *mon);
int monitor_set_cpu(Monitor *mon, int cpu_index);
int monitor_get_cpu_index(Monitor *mon);

int monitor_puts_locked(Monitor *mon, const char *str);
void monitor_flush_locked(Monitor *mon);

void *gpa2hva(MemoryRegion **p_mr, hwaddr addr, uint64_t size, Error **errp);

int error_vprintf_unless_qmp(const char *fmt, va_list ap) G_GNUC_PRINTF(1, 0);
int error_printf_unless_qmp(const char *fmt, ...) G_GNUC_PRINTF(1, 2);

#endif /* MONITOR_H */
