#include "qemu/osdep.h"
#include "system/gunyah_report.h"

#undef gh_report

typedef enum GhStatus {
    GH_STATUS_OK,
    GH_STATUS_FAILED,
    GH_STATUS_TIME,
    GH_STATUS_DEPEND,
    GH_STATUS_INFO,
    GH_STATUS_ASSERT,
    GH_STATUS_UNSUPP,
    GH_STATUS_ONCE,
    GH_STATUS_FROZEN,
    GH_STATUS_CONCUR,
} GhStatus;

static GhStatus gh_status_for(const char *msg)
{
    if (strstr(msg, "FAILED") || strstr(msg, "failed") ||
        strstr(msg, "ERROR") || strstr(msg, "Error") ||
        strstr(msg, "CRASHED") || strstr(msg, "WDT BITE") ||
        strstr(msg, "WILL SIGBUS") || strstr(msg, "invalid")) {
        return GH_STATUS_FAILED;
    }

    if (strstr(msg, "timeout") || strstr(msg, "Timed out")) {
        return GH_STATUS_TIME;
    }

    if (strstr(msg, "Dependency") || strstr(msg, "dependency")) {
        return GH_STATUS_DEPEND;
    }

    if (strstr(msg, "Assertion") || strstr(msg, "assert")) {
        return GH_STATUS_ASSERT;
    }

    if (strstr(msg, "not supported") || strstr(msg, "unsupported") ||
        strstr(msg, "ENOTTY")) {
        return GH_STATUS_UNSUPP;
    }

    if (strstr(msg, "started before") || strstr(msg, "cannot be started again")) {
        return GH_STATUS_ONCE;
    }

    if (strstr(msg, "frozen")) {
        return GH_STATUS_FROZEN;
    }

    if (strstr(msg, "concurrency") || strstr(msg, "CONCUR")) {
        return GH_STATUS_CONCUR;
    }

    if (strstr(msg, "WARNING") || strstr(msg, "Warning") ||
        strstr(msg, "warn")) {
        return GH_STATUS_INFO;
    }

    if (strstr(msg, " OK") || strstr(msg, "OK") ||
        strstr(msg, "opened") || strstr(msg, "created") ||
        strstr(msg, "loaded") || strstr(msg, "placed") ||
        strstr(msg, "registered") || strstr(msg, "armed") ||
        strstr(msg, "done") || strstr(msg, "started") ||
        strstr(msg, "enabled") || strstr(msg, "kept mapped")) {
        return GH_STATUS_OK;
    }

    return GH_STATUS_INFO;
}

static const char *gh_status_word(GhStatus status)
{
    switch (status) {
    case GH_STATUS_OK:
        return "  OK  ";
    case GH_STATUS_FAILED:
        return "FAILED";
    case GH_STATUS_TIME:
        return " TIME ";
    case GH_STATUS_DEPEND:
        return "DEPEND";
    case GH_STATUS_INFO:
        return " INFO ";
    case GH_STATUS_ASSERT:
        return "ASSERT";
    case GH_STATUS_UNSUPP:
        return "UNSUPP";
    case GH_STATUS_ONCE:
        return " ONCE ";
    case GH_STATUS_FROZEN:
        return "FROZEN";
    case GH_STATUS_CONCUR:
        return "CONCUR";
    default:
        return " INFO ";
    }
}

static const char *gh_status_color(GhStatus status)
{
    switch (status) {
    case GH_STATUS_OK:
        return gh_green;
    case GH_STATUS_FAILED:
    case GH_STATUS_TIME:
    case GH_STATUS_ONCE:
    case GH_STATUS_FROZEN:
    case GH_STATUS_CONCUR:
        return gh_red;
    case GH_STATUS_DEPEND:
    case GH_STATUS_ASSERT:
    case GH_STATUS_UNSUPP:
        return gh_yellow;
    case GH_STATUS_INFO:
    default:
        return gh_highlight;
    }
}

static const char *gh_file_name(const char *path)
{
    const char *slash = strrchr(path, '/');

    return slash ? slash + 1 : path;
}

static void gh_capitalize(char *msg)
{
    if (msg[0] >= 'a' && msg[0] <= 'z') {
        msg[0] -= 'a' - 'A';
    }
}

void gh_report(const char *file, const char *func, int line,
               const char *fmt, ...)
{
    va_list ap;
    char msg[1024];
    GhStatus status;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    gh_capitalize(msg);
    status = gh_status_for(msg);
    fprintf(stderr, "[%s%s%s] %s %s:%s:%d\n",
            gh_status_color(status), gh_status_word(status), gh_normal,
            msg, gh_file_name(file), func, line);
}
