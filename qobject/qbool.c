
#include "qemu/osdep.h"
#include "qobject/qbool.h"
#include "qobject-internal.h"

QBool *qbool_from_bool(bool value)
{
    QBool *qb;

    qb = g_malloc(sizeof(*qb));
    qobject_init(QOBJECT(qb), QTYPE_QBOOL);
    qb->value = value;

    return qb;
}

bool qbool_get_bool(const QBool *qb)
{
    return qb->value;
}

bool qbool_is_equal(const QObject *x, const QObject *y)
{
    return qobject_to(QBool, x)->value == qobject_to(QBool, y)->value;
}

void qbool_destroy_obj(QObject *obj)
{
    assert(obj != NULL);
    g_free(qobject_to(QBool, obj));
}

void qbool_unref(QBool *q)
{
    qobject_unref(q);
}
