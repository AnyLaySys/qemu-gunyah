
#include "qemu/osdep.h"
#include "qobject/qnum.h"
#include "qobject-internal.h"

QNum *qnum_from_int(int64_t value)
{
    QNum *qn = g_new(QNum, 1);

    qobject_init(QOBJECT(qn), QTYPE_QNUM);
    qn->kind = QNUM_I64;
    qn->u.i64 = value;

    return qn;
}

QNum *qnum_from_uint(uint64_t value)
{
    QNum *qn = g_new(QNum, 1);

    qobject_init(QOBJECT(qn), QTYPE_QNUM);
    qn->kind = QNUM_U64;
    qn->u.u64 = value;

    return qn;
}

QNum *qnum_from_double(double value)
{
    QNum *qn = g_new(QNum, 1);

    qobject_init(QOBJECT(qn), QTYPE_QNUM);
    qn->kind = QNUM_DOUBLE;
    qn->u.dbl = value;

    return qn;
}

bool qnum_get_try_int(const QNum *qn, int64_t *val)
{
    switch (qn->kind) {
    case QNUM_I64:
        *val = qn->u.i64;
        return true;
    case QNUM_U64:
        if (qn->u.u64 > INT64_MAX) {
            return false;
        }
        *val = qn->u.u64;
        return true;
    case QNUM_DOUBLE:
        return false;
    }

    g_assert_not_reached();
}

int64_t qnum_get_int(const QNum *qn)
{
    int64_t val;
    bool success = qnum_get_try_int(qn, &val);
    assert(success);
    return val;
}

bool qnum_get_try_uint(const QNum *qn, uint64_t *val)
{
    switch (qn->kind) {
    case QNUM_I64:
        if (qn->u.i64 < 0) {
            return false;
        }
        *val = qn->u.i64;
        return true;
    case QNUM_U64:
        *val = qn->u.u64;
        return true;
    case QNUM_DOUBLE:
        return false;
    }

    g_assert_not_reached();
}

uint64_t qnum_get_uint(const QNum *qn)
{
    uint64_t val;
    bool success = qnum_get_try_uint(qn, &val);
    assert(success);
    return val;
}

double qnum_get_double(QNum *qn)
{
    switch (qn->kind) {
    case QNUM_I64:
        return qn->u.i64;
    case QNUM_U64:
        return qn->u.u64;
    case QNUM_DOUBLE:
        return qn->u.dbl;
    }

    g_assert_not_reached();
}

char *qnum_to_string(QNum *qn)
{
    switch (qn->kind) {
    case QNUM_I64:
        return g_strdup_printf("%" PRId64, qn->u.i64);
    case QNUM_U64:
        return g_strdup_printf("%" PRIu64, qn->u.u64);
    case QNUM_DOUBLE:
        return g_strdup_printf("%.17g", qn->u.dbl);
    }

    g_assert_not_reached();
}

bool qnum_is_equal(const QObject *x, const QObject *y)
{
    QNum *num_x = qobject_to(QNum, x);
    QNum *num_y = qobject_to(QNum, y);

    switch (num_x->kind) {
    case QNUM_I64:
        switch (num_y->kind) {
        case QNUM_I64:
            return num_x->u.i64 == num_y->u.i64;
        case QNUM_U64:
            return num_x->u.i64 >= 0 && num_x->u.i64 == num_y->u.u64;
        case QNUM_DOUBLE:
            return false;
        }
        abort();
    case QNUM_U64:
        switch (num_y->kind) {
        case QNUM_I64:
            return qnum_is_equal(y, x);
        case QNUM_U64:
            return num_x->u.u64 == num_y->u.u64;
        case QNUM_DOUBLE:
            return false;
        }
        abort();
    case QNUM_DOUBLE:
        switch (num_y->kind) {
        case QNUM_I64:
        case QNUM_U64:
            return false;
        case QNUM_DOUBLE:
            return num_x->u.dbl == num_y->u.dbl;
        }
        abort();
    }

    abort();
}

void qnum_destroy_obj(QObject *obj)
{
    assert(obj != NULL);
    g_free(qobject_to(QNum, obj));
}

void qnum_unref(QNum *q)
{
    qobject_unref(q);
}
