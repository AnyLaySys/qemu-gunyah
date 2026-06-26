
#ifndef QNUM_H
#define QNUM_H

#include "qobject/qobject.h"

typedef enum {
    QNUM_I64,
    QNUM_U64,
    QNUM_DOUBLE
} QNumKind;

struct QNum {
    struct QObjectBase_ base;
    QNumKind kind;
    union {
        int64_t i64;
        uint64_t u64;
        double dbl;
    } u;
};

void qnum_unref(QNum *q);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QNum, qnum_unref)

QNum *qnum_from_int(int64_t value);
QNum *qnum_from_uint(uint64_t value);
QNum *qnum_from_double(double value);

bool qnum_get_try_int(const QNum *qn, int64_t *val);
int64_t qnum_get_int(const QNum *qn);

bool qnum_get_try_uint(const QNum *qn, uint64_t *val);
uint64_t qnum_get_uint(const QNum *qn);

double qnum_get_double(QNum *qn);

char *qnum_to_string(QNum *qn);

#endif /* QNUM_H */
