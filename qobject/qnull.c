
#include "qemu/osdep.h"
#include "qobject/qnull.h"
#include "qobject-internal.h"

QNull qnull_ = {
    .base = {
        .type = QTYPE_QNULL,
        .refcnt = 1,
    },
};

bool qnull_is_equal(const QObject *x, const QObject *y)
{
    return true;
}

void qnull_unref(QNull *q)
{
    qobject_unref(q);
}
