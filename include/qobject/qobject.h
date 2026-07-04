#ifndef QOBJECT_H
#define QOBJECT_H

#include "qapi/qapi-builtin-types.h"

struct QObjectBase_ {
    QType type;
    size_t refcnt;
};

struct QObject {
    struct QObjectBase_ base;
};

#define QOBJECT_INTERNAL(obj, _obj) ({                          \
    typeof(obj) _obj = (obj);                                   \
    _obj ? container_of(&_obj->base, QObject, base) : NULL;     \
})
#define QOBJECT(obj) QOBJECT_INTERNAL((obj), MAKE_IDENTIFIER(_obj))

#define QTYPE_CAST_TO_QNull     QTYPE_QNULL
#define QTYPE_CAST_TO_QNum      QTYPE_QNUM
#define QTYPE_CAST_TO_QString   QTYPE_QSTRING
#define QTYPE_CAST_TO_QDict     QTYPE_QDICT
#define QTYPE_CAST_TO_QList     QTYPE_QLIST
#define QTYPE_CAST_TO_QBool     QTYPE_QBOOL

QEMU_BUILD_BUG_MSG(QTYPE__MAX != 7,
                   "The QTYPE_CAST_TO_* list needs to be extended");

#define qobject_to(type, obj)                                       \
    ((type *)qobject_check_type(obj, glue(QTYPE_CAST_TO_, type)))

static inline void qobject_ref_impl(QObject *obj)
{
    if (obj) {
        obj->base.refcnt++;
    }
}

bool qobject_is_equal(const QObject *x, const QObject *y);

void qobject_destroy(QObject *obj);

static inline void qobject_unref_impl(QObject *obj)
{
    assert(!obj || obj->base.refcnt);
    if (obj && --obj->base.refcnt == 0) {
        qobject_destroy(obj);
    }
}

#define qobject_ref(obj) ({                     \
    typeof(obj) _o = (obj);                     \
    qobject_ref_impl(QOBJECT(_o));              \
    _o;                                         \
})

#define qobject_unref(obj) qobject_unref_impl(QOBJECT(obj))

static inline QType qobject_type(const QObject *obj)
{
    assert(QTYPE_NONE < obj->base.type && obj->base.type < QTYPE__MAX);
    return obj->base.type;
}

static inline QObject *qobject_check_type(const QObject *obj, QType type)
{
    if (obj && qobject_type(obj) == type) {
        return (QObject *)obj;
    } else {
        return NULL;
    }
}

#endif /* QOBJECT_H */
