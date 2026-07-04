
#ifndef QEMU_LOG_FOR_TRACE_H
#define QEMU_LOG_FOR_TRACE_H

extern int qemu_loglevel;

#define LOG_TRACE          (1 << 15)

static inline bool qemu_loglevel_mask(int mask)
{
    return (qemu_loglevel & mask) != 0;
}

void G_GNUC_PRINTF(1, 2) qemu_log(const char *fmt, ...);

#endif
