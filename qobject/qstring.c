
#include "qemu/osdep.h"
#include "qobject/qstring.h"
#include "qobject-internal.h"

QString *qstring_new(void)
{
    return qstring_from_str("");
}

QString *qstring_from_substr(const char *str, size_t start, size_t end)
{
    QString *qstring;

    assert(start <= end);
    qstring = g_malloc(sizeof(*qstring));
    qobject_init(QOBJECT(qstring), QTYPE_QSTRING);
    qstring->string = g_strndup(str + start, end - start);
    return qstring;
}

QString *qstring_from_str(const char *str)
{
    return qstring_from_substr(str, 0, strlen(str));
}


QString *qstring_from_gstring(GString *gstr)
{
    QString *qstring;

    qstring = g_malloc(sizeof(*qstring));
    qobject_init(QOBJECT(qstring), QTYPE_QSTRING);
    qstring->string = g_string_free(gstr, false);
    return qstring;
}


const char *qstring_get_str(const QString *qstring)
{
    return qstring->string;
}

bool qstring_is_equal(const QObject *x, const QObject *y)
{
    return !strcmp(qobject_to(QString, x)->string,
                   qobject_to(QString, y)->string);
}

void qstring_destroy_obj(QObject *obj)
{
    QString *qs;

    assert(obj != NULL);
    qs = qobject_to(QString, obj);
    g_free((char *)qs->string);
    g_free(qs);
}

void qstring_unref(QString *q)
{
    qobject_unref(q);
}
