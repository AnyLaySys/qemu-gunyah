
#include "qemu/osdep.h"
#include "monitor/monitor.h"
#include "qemu/qemu-print.h"

int qemu_vprintf(const char *fmt, va_list ap)
{
    Monitor *cur_mon = monitor_cur();
    if (cur_mon) {
        return monitor_vprintf(cur_mon, fmt, ap);
    }
    return vprintf(fmt, ap);
}

int qemu_printf(const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = qemu_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int qemu_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    if (!stream) {
        return monitor_vprintf(monitor_cur(), fmt, ap);
    }
    return vfprintf(stream, fmt, ap);
}

int qemu_fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    int ret;

    va_start(ap, fmt);
    ret = qemu_vfprintf(stream, fmt, ap);
    va_end(ap);
    return ret;
}
