
#ifndef QBOOL_H
#define QBOOL_H

#include "qobject/qobject.h"

struct QBool {
    struct QObjectBase_ base;
    bool value;
};

void qbool_unref(QBool *q);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QBool, qbool_unref)

QBool *qbool_from_bool(bool value);
bool qbool_get_bool(const QBool *qb);

#endif /* QBOOL_H */
