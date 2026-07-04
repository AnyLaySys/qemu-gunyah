
#ifndef QEMU_OPTION_INT_H
#define QEMU_OPTION_INT_H

#include "qemu/option.h"
#include "qemu/error-report.h"

struct QemuOpt {
    char *name;
    char *str;

    const QemuOptDesc *desc;
    union {
        bool boolean;
        uint64_t uint;
    } value;

    QemuOpts     *opts;
    QTAILQ_ENTRY(QemuOpt) next;
};

struct QemuOpts {
    char *id;
    QemuOptsList *list;
    Location loc;
    QTAILQ_HEAD(, QemuOpt) head;
    QTAILQ_ENTRY(QemuOpts) next;
};

#endif
