
#ifndef QNULL_H
#define QNULL_H

#include "qobject/qobject.h"

struct QNull {
    struct QObjectBase_ base;
};

extern QNull qnull_;

static inline QNull *qnull(void)
{
    return qobject_ref(&qnull_);
}

void qnull_unref(QNull *q);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QNull, qnull_unref)

#endif /* QNULL_H */
