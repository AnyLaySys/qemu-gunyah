#include "qemu/osdep.h"
#include "monitor/monitor.h"
#include "qemu/error-report.h"

typedef enum {
    REPORT_TYPE_ERROR, REPORT_TYPE_WARNING, REPORT_TYPE_INFO,
} report_type;
bool message_with_timestamp;
bool error_with_guestname;
const char *error_guest_name;

int error_printf(const char *fmt, ...) {
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = error_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

static Location std_loc = {.kind = LOC_NONE};
static Location *cur_loc = &std_loc;

Location *loc_push_restore(Location *loc) {
    assert(!loc->prev);
    loc->prev = cur_loc;
    cur_loc = loc;
    return loc;
}

Location *loc_push_none(Location *loc) {
    loc->kind = LOC_NONE;
    loc->prev = NULL;
    return loc_push_restore(loc);
}

Location *loc_pop(Location *loc) {
    assert(cur_loc == loc && loc->prev);
    cur_loc = loc->prev;
    loc->prev = NULL;
    return loc;
}

Location *loc_save(Location *loc) {
    *loc = *cur_loc;
    loc->prev = NULL;
    return loc;
}

void loc_restore(Location *loc) {
    Location *prev = cur_loc->prev;
    assert(!loc->prev);
    *cur_loc = *loc;
    cur_loc->prev = prev;
}

void loc_set_none(void) {
    cur_loc->kind = LOC_NONE;
}

void loc_set_cmdline(char **argv, int idx, int cnt) {
    cur_loc->kind = LOC_CMDLINE;
    cur_loc->num = cnt;
    cur_loc->ptr = argv + idx;
}

void loc_set_file(const char *fname, int lno) {
    assert(fname || cur_loc->kind == LOC_FILE);
    cur_loc->kind = LOC_FILE;
    cur_loc->num = lno;
    if (fname) {
        cur_loc->ptr = fname;
    }
}

static void print_loc(void) {
    const char *sep = "";
    int i;
    const char *const *argp;

    switch (cur_loc->kind) {
        case LOC_CMDLINE:
            argp = cur_loc->ptr;
            for (i = 0; i < cur_loc->num; i++) {
                error_printf("%s%s", sep, argp[i]);
                sep = " ";
            }
            error_printf(": ");
            break;
        case LOC_FILE:
            error_printf("%s:", (const char *) cur_loc->ptr);
            if (cur_loc->num) {
                error_printf("%d:", cur_loc->num);
            }
            error_printf(" ");
            break;
        default:
            error_printf("%s", sep);
    }
}

static char *real_time_iso8601(void) {
    g_autoptr(GDateTime)
    dt = g_date_time_new_now_utc();
    return g_date_time_format_iso8601(dt);
}

G_GNUC_PRINTF(

2, 0)

static void vreport(report_type type, const char *fmt, va_list ap) {
    gchar *timestr;
    if (message_with_timestamp && !monitor_cur()) {
        timestr = real_time_iso8601();
        error_printf("%s ", timestr);
        g_free(timestr);
    }
    if (error_with_guestname && error_guest_name && !monitor_cur()) {
        error_printf("%s ", error_guest_name);
    }
    print_loc();
    switch (type) {
        case REPORT_TYPE_ERROR:
            break;
        case REPORT_TYPE_WARNING:
            error_printf("warning: ");
            break;
        case REPORT_TYPE_INFO:
            error_printf("info: ");
            break;
    }
    error_vprintf(fmt, ap);
    error_printf("\n");
}

void error_vreport(const char *fmt, va_list ap) {
    vreport(REPORT_TYPE_ERROR, fmt, ap);
}

void warn_vreport(const char *fmt, va_list ap) {
    vreport(REPORT_TYPE_WARNING, fmt, ap);
}

void info_vreport(const char *fmt, va_list ap) {
    vreport(REPORT_TYPE_INFO, fmt, ap);
}

void error_report(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport(REPORT_TYPE_ERROR, fmt, ap);
    va_end(ap);
}

void warn_report(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport(REPORT_TYPE_WARNING, fmt, ap);
    va_end(ap);
}

void info_report(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreport(REPORT_TYPE_INFO, fmt, ap);
    va_end(ap);
}

bool error_report_once_cond(bool *printed, const char *fmt, ...) {
    va_list ap;
    assert(printed);
    if (*printed) {
        return false;
    }
    *printed = true;
    va_start(ap, fmt);
    vreport(REPORT_TYPE_ERROR, fmt, ap);
    va_end(ap);
    return true;
}

bool warn_report_once_cond(bool *printed, const char *fmt, ...) {
    va_list ap;
    assert(printed);
    if (*printed) {
        return false;
    }
    *printed = true;
    va_start(ap, fmt);
    vreport(REPORT_TYPE_WARNING, fmt, ap);
    va_end(ap);
    return true;
}

static char *qemu_glog_domains;

static void qemu_log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
                          gpointer user_data) {
    switch (log_level & G_LOG_LEVEL_MASK) {
        case G_LOG_LEVEL_DEBUG:
        case G_LOG_LEVEL_INFO:
            if (qemu_glog_domains == NULL) {
                break;
            }
            if (strcmp(qemu_glog_domains, "all") != 0 &&
                (log_domain == NULL || !strstr(qemu_glog_domains, log_domain))) {
                break;
            }
        case G_LOG_LEVEL_MESSAGE:
            info_report("%s%s%s", log_domain ?: "", log_domain ? ": " : "", message);
            break;
        case G_LOG_LEVEL_WARNING:
            warn_report("%s%s%s", log_domain ?: "", log_domain ? ": " : "", message);
            break;
        case G_LOG_LEVEL_CRITICAL:
        case G_LOG_LEVEL_ERROR:
            error_report("%s%s%s", log_domain ?: "", log_domain ? ": " : "", message);
            break;
    }
}

void error_init(const char *argv0) {
    const char *p = strrchr(argv0, '/');
    g_set_prgname(p ? p + 1 : argv0);
    g_log_set_default_handler(qemu_log_func, NULL);
    g_warn_if_fail(qemu_glog_domains == NULL);
    qemu_glog_domains = g_strdup(g_getenv("G_MESSAGES_DEBUG"));
}
