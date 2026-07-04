
#include "qemu/osdep.h"
#include "block/qdict.h"
#include "qobject/qbool.h"
#include "qobject/qlist.h"
#include "qobject/qnum.h"
#include "qobject/qstring.h"
#include "qapi/qobject-input-visitor.h"
#include "qemu/cutils.h"
#include "qapi/error.h"

void qdict_copy_default(QDict *dst, QDict *src, const char *key)
{
    QObject *val;

    if (qdict_haskey(dst, key)) {
        return;
    }

    val = qdict_get(src, key);
    if (val) {
        qdict_put_obj(dst, key, qobject_ref(val));
    }
}

void qdict_set_default_str(QDict *dst, const char *key, const char *val)
{
    if (qdict_haskey(dst, key)) {
        return;
    }

    qdict_put_str(dst, key, val);
}

static void qdict_flatten_qdict(QDict *qdict, QDict *target,
                                const char *prefix);

static void qdict_flatten_qlist(QList *qlist, QDict *target, const char *prefix)
{
    QObject *value;
    const QListEntry *entry;
    QDict *dict_val;
    QList *list_val;
    char *new_key;
    int i;

    assert(prefix);

    entry = qlist_first(qlist);

    for (i = 0; entry; entry = qlist_next(entry), i++) {
        value = qlist_entry_obj(entry);
        dict_val = qobject_to(QDict, value);
        list_val = qobject_to(QList, value);
        new_key = g_strdup_printf("%s.%i", prefix, i);

        if (dict_val && qdict_size(dict_val)) {
            qdict_flatten_qdict(dict_val, target, new_key);
        } else if (list_val && !qlist_empty(list_val)) {
            qdict_flatten_qlist(list_val, target, new_key);
        } else {
            qdict_put_obj(target, new_key, qobject_ref(value));
        }

        g_free(new_key);
    }
}

static void qdict_flatten_qdict(QDict *qdict, QDict *target, const char *prefix)
{
    QObject *value;
    const QDictEntry *entry, *next;
    QDict *dict_val;
    QList *list_val;
    char *key, *new_key;

    entry = qdict_first(qdict);

    while (entry != NULL) {
        next = qdict_next(qdict, entry);
        value = qdict_entry_value(entry);
        dict_val = qobject_to(QDict, value);
        list_val = qobject_to(QList, value);

        if (prefix) {
            key = new_key = g_strdup_printf("%s.%s", prefix, entry->key);
        } else {
            key = entry->key;
            new_key = NULL;
        }

        if (dict_val && qdict_size(dict_val)) {
            qdict_flatten_qdict(dict_val, target, key);
            if (target == qdict) {
                qdict_del(qdict, entry->key);
            }
        } else if (list_val && !qlist_empty(list_val)) {
            qdict_flatten_qlist(list_val, target, key);
            if (target == qdict) {
                qdict_del(qdict, entry->key);
            }
        } else if (target != qdict) {
            qdict_put_obj(target, key, qobject_ref(value));
        }

        g_free(new_key);
        entry = next;
    }
}

void qdict_flatten(QDict *qdict)
{
    qdict_flatten_qdict(qdict, qdict, NULL);
}

void qdict_extract_subqdict(QDict *src, QDict **dst, const char *start)

{
    const QDictEntry *entry, *next;
    const char *p;

    if (dst) {
        *dst = qdict_new();
    }
    entry = qdict_first(src);

    while (entry != NULL) {
        next = qdict_next(src, entry);
        if (strstart(entry->key, start, &p)) {
            if (dst) {
                qdict_put_obj(*dst, p, qobject_ref(entry->value));
            }
            qdict_del(src, entry->key);
        }
        entry = next;
    }
}

static int qdict_count_prefixed_entries(const QDict *src, const char *start)
{
    const QDictEntry *entry;
    int count = 0;

    for (entry = qdict_first(src); entry; entry = qdict_next(src, entry)) {
        if (strstart(entry->key, start, NULL)) {
            if (count == INT_MAX) {
                return -ERANGE;
            }
            count++;
        }
    }

    return count;
}

void qdict_array_split(QDict *src, QList **dst)
{
    unsigned i;

    *dst = qlist_new();

    for (i = 0; i < UINT_MAX; i++) {
        QObject *subqobj;
        bool is_subqdict;
        QDict *subqdict;
        char indexstr[32], prefix[32];
        size_t snprintf_ret;

        snprintf_ret = snprintf(indexstr, 32, "%u", i);
        assert(snprintf_ret < 32);

        subqobj = qdict_get(src, indexstr);

        snprintf_ret = snprintf(prefix, 32, "%u.", i);
        assert(snprintf_ret < 32);

        is_subqdict = qdict_count_prefixed_entries(src, prefix);

        if (!subqobj == !is_subqdict) {
            break;
        }

        if (is_subqdict) {
            qdict_extract_subqdict(src, &subqdict, prefix);
            assert(qdict_size(subqdict) > 0);
            qlist_append_obj(*dst, QOBJECT(subqdict));
        } else {
            qobject_ref(subqobj);
            qdict_del(src, indexstr);
            qlist_append_obj(*dst, subqobj);
        }
    }
}

static void qdict_split_flat_key(const char *key, char **prefix,
                                 const char **suffix)
{
    const char *separator;
    size_t i, j;

    separator = NULL;
    do {
        if (separator) {
            separator += 2;
        } else {
            separator = key;
        }
        separator = strchr(separator, '.');
    } while (separator && separator[1] == '.');

    if (separator) {
        *prefix = g_strndup(key, separator - key);
        *suffix = separator + 1;
    } else {
        *prefix = g_strdup(key);
        *suffix = NULL;
    }

    for (i = 0, j = 0; (*prefix)[i] != '\0'; i++, j++) {
        if ((*prefix)[i] == '.') {
            assert((*prefix)[i + 1] == '.');
            i++;
        }
        (*prefix)[j] = (*prefix)[i];
    }
    (*prefix)[j] = '\0';
}

static int qdict_is_list(QDict *maybe_list, Error **errp)
{
    const QDictEntry *ent;
    ssize_t len = 0;
    ssize_t max = -1;
    int is_list = -1;
    int64_t val;

    for (ent = qdict_first(maybe_list); ent != NULL;
         ent = qdict_next(maybe_list, ent)) {
        int is_index = !qemu_strtoi64(ent->key, NULL, 10, &val);

        if (is_list == -1) {
            is_list = is_index;
        }

        if (is_index != is_list) {
            error_setg(errp, "Cannot mix list and non-list keys");
            return -1;
        }

        if (is_index) {
            len++;
            if (val > max) {
                max = val;
            }
        }
    }

    if (is_list == -1) {
        assert(!qdict_size(maybe_list));
        is_list = 0;
    }

    if (len != (max + 1)) {
        error_setg(errp, "List indices are not contiguous, "
                   "saw %zd elements but %zd largest index",
                   len, max);
        return -1;
    }

    return is_list;
}

QObject *qdict_crumple(const QDict *src, Error **errp)
{
    const QDictEntry *ent;
    QDict *two_level, *multi_level = NULL, *child_dict;
    QDict *dict_val;
    QList *list_val;
    QObject *dst = NULL, *child;
    size_t i;
    char *prefix = NULL;
    const char *suffix = NULL;
    int is_list;

    two_level = qdict_new();

    for (ent = qdict_first(src); ent != NULL; ent = qdict_next(src, ent)) {
        dict_val = qobject_to(QDict, ent->value);
        list_val = qobject_to(QList, ent->value);
        if ((dict_val && qdict_size(dict_val))
            || (list_val && !qlist_empty(list_val))) {
            error_setg(errp, "Value %s is not flat", ent->key);
            goto error;
        }

        qdict_split_flat_key(ent->key, &prefix, &suffix);
        child = qdict_get(two_level, prefix);
        child_dict = qobject_to(QDict, child);

        if (child) {
            if (!child_dict || !suffix) {
                error_setg(errp, "Cannot mix scalar and non-scalar keys");
                goto error;
            }
        }

        if (suffix) {
            if (!child_dict) {
                child_dict = qdict_new();
                qdict_put(two_level, prefix, child_dict);
            }
            qdict_put_obj(child_dict, suffix, qobject_ref(ent->value));
        } else {
            qdict_put_obj(two_level, prefix, qobject_ref(ent->value));
        }

        g_free(prefix);
        prefix = NULL;
    }

    multi_level = qdict_new();
    for (ent = qdict_first(two_level); ent != NULL;
         ent = qdict_next(two_level, ent)) {
        dict_val = qobject_to(QDict, ent->value);
        if (dict_val && qdict_size(dict_val)) {
            child = qdict_crumple(dict_val, errp);
            if (!child) {
                goto error;
            }

            qdict_put_obj(multi_level, ent->key, child);
        } else {
            qdict_put_obj(multi_level, ent->key, qobject_ref(ent->value));
        }
    }
    qobject_unref(two_level);
    two_level = NULL;

    is_list = qdict_is_list(multi_level, errp);
    if (is_list < 0) {
        goto error;
    }

    if (is_list) {
        dst = QOBJECT(qlist_new());

        for (i = 0; i < qdict_size(multi_level); i++) {
            char *key = g_strdup_printf("%zu", i);

            child = qdict_get(multi_level, key);
            g_free(key);

            if (!child) {
                error_setg(errp, "Missing list index %zu", i);
                goto error;
            }

            qlist_append_obj(qobject_to(QList, dst), qobject_ref(child));
        }
        qobject_unref(multi_level);
        multi_level = NULL;
    } else {
        dst = QOBJECT(multi_level);
    }

    return dst;

 error:
    g_free(prefix);
    qobject_unref(multi_level);
    qobject_unref(two_level);
    qobject_unref(dst);
    return NULL;
}

static QObject *qdict_crumple_for_keyval_qiv(QDict *src, Error **errp)
{
    QDict *tmp = NULL;
    char *buf;
    const char *s;
    const QDictEntry *ent;
    QObject *dst;

    for (ent = qdict_first(src); ent; ent = qdict_next(src, ent)) {
        buf = NULL;
        switch (qobject_type(ent->value)) {
        case QTYPE_QNULL:
        case QTYPE_QSTRING:
            continue;
        case QTYPE_QNUM:
            s = buf = qnum_to_string(qobject_to(QNum, ent->value));
            break;
        case QTYPE_QDICT:
        case QTYPE_QLIST:
            continue;
        case QTYPE_QBOOL:
            s = qbool_get_bool(qobject_to(QBool, ent->value))
                ? "on" : "off";
            break;
        default:
            abort();
        }

        if (!tmp) {
            tmp = qdict_clone_shallow(src);
        }
        qdict_put_str(tmp, ent->key, s);
        g_free(buf);
    }

    dst = qdict_crumple(tmp ?: src, errp);
    qobject_unref(tmp);
    return dst;
}

int qdict_array_entries(QDict *src, const char *subqdict)
{
    const QDictEntry *entry;
    unsigned i;
    unsigned entries = 0;
    size_t subqdict_len = strlen(subqdict);

    assert(!subqdict_len || subqdict[subqdict_len - 1] == '.');

    for (i = 0; i < INT_MAX; i++) {
        QObject *subqobj;
        int subqdict_entries;
        char *prefix = g_strdup_printf("%s%u.", subqdict, i);

        subqdict_entries = qdict_count_prefixed_entries(src, prefix);

        prefix[strlen(prefix) - 1] = 0;
        subqobj = qdict_get(src, prefix);

        g_free(prefix);

        if (subqdict_entries < 0) {
            return subqdict_entries;
        }

        if (subqobj && subqdict_entries) {
            return -EINVAL;
        } else if (!subqobj && !subqdict_entries) {
            break;
        }

        entries += subqdict_entries ? subqdict_entries : 1;
    }

    for (entry = qdict_first(src); entry; entry = qdict_next(src, entry)) {
        if (!strstart(qdict_entry_key(entry), subqdict, NULL)) {
            entries++;
        }
    }

    if (qdict_size(src) != entries) {
        return -EINVAL;
    }

    return i;
}

void qdict_join(QDict *dest, QDict *src, bool overwrite)
{
    const QDictEntry *entry, *next;

    entry = qdict_first(src);
    while (entry) {
        next = qdict_next(src, entry);

        if (overwrite || !qdict_haskey(dest, entry->key)) {
            qdict_put_obj(dest, entry->key, qobject_ref(entry->value));
            qdict_del(src, entry->key);
        }

        entry = next;
    }
}

bool qdict_rename_keys(QDict *qdict, const QDictRenames *renames, Error **errp)
{
    QObject *qobj;

    while (renames->from) {
        if (qdict_haskey(qdict, renames->from)) {
            if (qdict_haskey(qdict, renames->to)) {
                error_setg(errp, "'%s' and its alias '%s' can't be used at the "
                           "same time", renames->to, renames->from);
                return false;
            }

            qobj = qdict_get(qdict, renames->from);
            qdict_put_obj(qdict, renames->to, qobject_ref(qobj));
            qdict_del(qdict, renames->from);
        }

        renames++;
    }
    return true;
}

Visitor *qobject_input_visitor_new_flat_confused(QDict *qdict,
                                                 Error **errp)
{
    QObject *crumpled;
    Visitor *v;

    crumpled = qdict_crumple_for_keyval_qiv(qdict, errp);
    if (!crumpled) {
        return NULL;
    }

    v = qobject_input_visitor_new_keyval(crumpled);
    qobject_unref(crumpled);
    return v;
}
