
#include "qemu/osdep.h"
#include "monitor-internal.h"
#include "qapi/error.h"
#include "qapi/opts-visitor.h"
#include "qapi/qapi-emit-events.h"
#include "qapi/qapi-visit-control.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "trace.h"

IOThread *mon_iothread;

QemuMutex monitor_lock;
static GHashTable *coroutine_mon; /* Maps Coroutine* to Monitor* */

MonitorList mon_list;
static bool monitor_destroyed;

Monitor *monitor_cur(void)
{
    Monitor *mon;

    qemu_mutex_lock(&monitor_lock);
    mon = g_hash_table_lookup(coroutine_mon, qemu_coroutine_self());
    qemu_mutex_unlock(&monitor_lock);

    return mon;
}

Monitor *monitor_set_cur(Coroutine *co, Monitor *mon)
{
    Monitor *old_monitor = monitor_cur();

    qemu_mutex_lock(&monitor_lock);
    if (mon) {
        g_hash_table_replace(coroutine_mon, co, mon);
    } else {
        g_hash_table_remove(coroutine_mon, co);
    }
    qemu_mutex_unlock(&monitor_lock);

    return old_monitor;
}

bool monitor_cur_is_qmp(void)
{
    return false;
}

static inline bool monitor_uses_readline(const MonitorHMP *mon)
{
    return mon->use_readline;
}

static inline bool monitor_is_hmp_non_interactive(const Monitor *mon)
{
    return !monitor_uses_readline(container_of(mon, MonitorHMP, common));
}

static gboolean monitor_unblocked(void *do_not_use, GIOCondition cond,
                                  void *opaque)
{
    Monitor *mon = opaque;

    QEMU_LOCK_GUARD(&mon->mon_lock);
    mon->out_watch = 0;
    monitor_flush_locked(mon);
    return G_SOURCE_REMOVE;
}

void monitor_flush_locked(Monitor *mon)
{
    int rc;
    size_t len;
    const char *buf;

    if (mon->skip_flush) {
        return;
    }

    buf = mon->outbuf->str;
    len = mon->outbuf->len;

    if (len && !mon->mux_out) {
        rc = qemu_chr_fe_write(&mon->chr, (const uint8_t *) buf, len);
        if ((rc < 0 && errno != EAGAIN) || (rc == len)) {
            g_string_truncate(mon->outbuf, 0);
            return;
        }
        if (rc > 0) {
            g_string_erase(mon->outbuf, 0, rc);
        }
        if (mon->out_watch == 0) {
            mon->out_watch =
                qemu_chr_fe_add_watch(&mon->chr, G_IO_OUT | G_IO_HUP,
                                      monitor_unblocked, mon);
        }
    }
}

void monitor_flush(Monitor *mon)
{
    QEMU_LOCK_GUARD(&mon->mon_lock);
    monitor_flush_locked(mon);
}

int monitor_puts_locked(Monitor *mon, const char *str)
{
    int i;
    char c;

    for (i = 0; str[i]; i++) {
        c = str[i];
        if (c == '\n') {
            g_string_append_c(mon->outbuf, '\r');
        }
        g_string_append_c(mon->outbuf, c);
        if (c == '\n') {
            monitor_flush_locked(mon);
        }
    }

    return i;
}

int monitor_puts(Monitor *mon, const char *str)
{
    QEMU_LOCK_GUARD(&mon->mon_lock);
    return monitor_puts_locked(mon, str);
}

int monitor_vprintf(Monitor *mon, const char *fmt, va_list ap)
{
    char *buf;
    int n;

    if (!mon) {
        return -1;
    }

    buf = g_strdup_vprintf(fmt, ap);
    n = monitor_puts(mon, buf);
    g_free(buf);
    return n;
}

int monitor_printf(Monitor *mon, const char *fmt, ...)
{
    int ret;

    va_list ap;
    va_start(ap, fmt);
    ret = monitor_vprintf(mon, fmt, ap);
    va_end(ap);
    return ret;
}

void monitor_printc(Monitor *mon, int c)
{
    monitor_printf(mon, "'");
    switch(c) {
    case '\'':
        monitor_printf(mon, "\\'");
        break;
    case '\\':
        monitor_printf(mon, "\\\\");
        break;
    case '\n':
        monitor_printf(mon, "\\n");
        break;
    case '\r':
        monitor_printf(mon, "\\r");
        break;
    default:
        if (c >= 32 && c <= 126) {
            monitor_printf(mon, "%c", c);
        } else {
            monitor_printf(mon, "\\x%02x", c);
        }
        break;
    }
    monitor_printf(mon, "'");
}

int error_vprintf(const char *fmt, va_list ap)
{
    Monitor *cur_mon = monitor_cur();

    if (cur_mon) {
        return monitor_vprintf(cur_mon, fmt, ap);
    }
    return vfprintf(stderr, fmt, ap);
}

int error_vprintf_unless_qmp(const char *fmt, va_list ap)
{
    Monitor *cur_mon = monitor_cur();

    if (!cur_mon) {
        return vfprintf(stderr, fmt, ap);
    }
    return monitor_vprintf(cur_mon, fmt, ap);
}

int error_printf_unless_qmp(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = error_vprintf_unless_qmp(fmt, ap);
    va_end(ap);
    return ret;
}

void qapi_event_emit(QAPIEvent event, QDict *qdict)
{
}

int monitor_suspend(Monitor *mon)
{
    if (monitor_is_hmp_non_interactive(mon)) {
        return -ENOTTY;
    }

    qatomic_inc(&mon->suspend_cnt);

    if (mon->use_io_thread) {
        aio_notify(iothread_get_aio_context(mon_iothread));
    }

    trace_monitor_suspend(mon, 1);
    return 0;
}

static void monitor_accept_input(void *opaque)
{
    Monitor *mon = opaque;

    qemu_mutex_lock(&mon->mon_lock);
    if (mon->reset_seen) {
        MonitorHMP *hmp_mon = container_of(mon, MonitorHMP, common);
        assert(hmp_mon->rs);
        readline_restart(hmp_mon->rs);
        qemu_mutex_unlock(&mon->mon_lock);
        readline_show_prompt(hmp_mon->rs);
    } else {
        qemu_mutex_unlock(&mon->mon_lock);
    }

    qemu_chr_fe_accept_input(&mon->chr);
}

void monitor_resume(Monitor *mon)
{
    if (monitor_is_hmp_non_interactive(mon)) {
        return;
    }

    if (qatomic_dec_fetch(&mon->suspend_cnt) == 0) {
        AioContext *ctx;

        if (mon->use_io_thread) {
            ctx = iothread_get_aio_context(mon_iothread);
        } else {
            ctx = qemu_get_aio_context();
        }

        aio_bh_schedule_oneshot(ctx, monitor_accept_input, mon);
    }

    trace_monitor_suspend(mon, -1);
}

int monitor_can_read(void *opaque)
{
    Monitor *mon = opaque;

    return !qatomic_read(&mon->suspend_cnt);
}

void monitor_list_append(Monitor *mon)
{
    qemu_mutex_lock(&monitor_lock);
    if (!monitor_destroyed) {
        QTAILQ_INSERT_HEAD(&mon_list, mon, entry);
        mon = NULL;
    }
    qemu_mutex_unlock(&monitor_lock);

    if (mon) {
        monitor_data_destroy(mon);
        g_free(mon);
    }
}

static void monitor_iothread_init(void)
{
    mon_iothread = iothread_create("mon_iothread", &error_abort);
}

void monitor_data_init(Monitor *mon, bool skip_flush, bool use_io_thread)
{
    if (use_io_thread && !mon_iothread) {
        monitor_iothread_init();
    }
    qemu_mutex_init(&mon->mon_lock);
    mon->outbuf = g_string_new(NULL);
    mon->skip_flush = skip_flush;
    mon->use_io_thread = use_io_thread;
}

void monitor_data_destroy(Monitor *mon)
{
    g_free(mon->mon_cpu_path);
    qemu_chr_fe_deinit(&mon->chr, false);
    readline_free(container_of(mon, MonitorHMP, common)->rs);
    g_string_free(mon->outbuf, true);
    qemu_mutex_destroy(&mon->mon_lock);
}

void monitor_cleanup(void)
{
    if (mon_iothread) {
        iothread_stop(mon_iothread);
    }

    qemu_mutex_lock(&monitor_lock);
    monitor_destroyed = true;
    while (!QTAILQ_EMPTY(&mon_list)) {
        Monitor *mon = QTAILQ_FIRST(&mon_list);
        QTAILQ_REMOVE(&mon_list, mon, entry);
        qemu_mutex_unlock(&monitor_lock);
        monitor_flush(mon);
        monitor_data_destroy(mon);
        qemu_mutex_lock(&monitor_lock);
        g_free(mon);
    }
    qemu_mutex_unlock(&monitor_lock);

    if (mon_iothread) {
        iothread_destroy(mon_iothread);
        mon_iothread = NULL;
    }
}

void monitor_init_globals(void)
{
    qemu_mutex_init(&monitor_lock);
    coroutine_mon = g_hash_table_new(NULL, NULL);
}

int monitor_init(MonitorOptions *opts, Error **errp)
{
    ERRP_GUARD();
    Chardev *chr;

    chr = qemu_chr_find(opts->chardev);
    if (chr == NULL) {
        error_setg(errp, "chardev \"%s\" not found", opts->chardev);
        return -1;
    }

    monitor_init_hmp(chr, true, errp);

    return *errp ? -1 : 0;
}

int monitor_init_opts(QemuOpts *opts, Error **errp)
{
    Visitor *v;
    MonitorOptions *options;
    int ret;

    v = opts_visitor_new(opts);
    visit_type_MonitorOptions(v, NULL, &options, errp);
    visit_free(v);
    if (!options) {
        return -1;
    }

    ret = monitor_init(options, errp);
    qapi_free_MonitorOptions(options);
    return ret;
}

QemuOptsList qemu_mon_opts = {
    .name = "mon",
    .implied_opt_name = "chardev",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_mon_opts.head),
    .desc = {
        {
            .name = "chardev",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};
