
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/string-input-visitor.h"
#include "qapi/visitor-impl.h"
#include "qapi/qmp/qerror.h"
#include "qobject/qnull.h"
#include "qemu/option.h"
#include "qemu/cutils.h"

typedef enum ListMode {
    LM_NONE,
    LM_UNPARSED,
    LM_INT64_RANGE,
    LM_UINT64_RANGE,
    LM_END,
} ListMode;

#define RANGE_MAX_ELEMENTS 65536

typedef union RangeElement {
    int64_t i64;
    uint64_t u64;
} RangeElement;

struct StringInputVisitor
{
    Visitor visitor;

    ListMode lm;
    RangeElement rangeNext;
    RangeElement rangeEnd;
    const char *unparsed_string;
    void *list;

    const char *string;
};

static StringInputVisitor *to_siv(Visitor *v)
{
    return container_of(v, StringInputVisitor, visitor);
}

static bool start_list(Visitor *v, const char *name, GenericList **list,
                       size_t size, Error **errp)
{
    StringInputVisitor *siv = to_siv(v);

    assert(siv->lm == LM_NONE);
    siv->list = list;
    siv->unparsed_string = siv->string;

    if (!siv->string[0]) {
        if (list) {
            *list = NULL;
        }
        siv->lm = LM_END;
    } else {
        if (list) {
            *list = g_malloc0(size);
        }
        siv->lm = LM_UNPARSED;
    }
    return true;
}

static GenericList *next_list(Visitor *v, GenericList *tail, size_t size)
{
    StringInputVisitor *siv = to_siv(v);

    switch (siv->lm) {
    case LM_END:
        return NULL;
    case LM_INT64_RANGE:
    case LM_UINT64_RANGE:
    case LM_UNPARSED:
        break;
    default:
        abort();
    }

    tail->next = g_malloc0(size);
    return tail->next;
}

static bool check_list(Visitor *v, Error **errp)
{
    const StringInputVisitor *siv = to_siv(v);

    switch (siv->lm) {
    case LM_INT64_RANGE:
    case LM_UINT64_RANGE:
    case LM_UNPARSED:
        error_setg(errp, "Fewer list elements expected");
        return false;
    case LM_END:
        return true;
    default:
        abort();
    }
}

static void end_list(Visitor *v, void **obj)
{
    StringInputVisitor *siv = to_siv(v);

    assert(siv->lm != LM_NONE);
    assert(siv->list == obj);
    siv->list = NULL;
    siv->unparsed_string = NULL;
    siv->lm = LM_NONE;
}

static int try_parse_int64_list_entry(StringInputVisitor *siv, int64_t *obj)
{
    const char *endptr;
    int64_t start, end;

    if (qemu_strtoi64(siv->unparsed_string, &endptr, 0, &start)) {
        return -EINVAL;
    }
    end = start;

    switch (endptr[0]) {
    case '\0':
        siv->unparsed_string = endptr;
        break;
    case ',':
        siv->unparsed_string = endptr + 1;
        break;
    case '-':
        if (qemu_strtoi64(endptr + 1, &endptr, 0, &end)) {
            return -EINVAL;
        }
        if (start > end || end - start >= RANGE_MAX_ELEMENTS) {
            return -EINVAL;
        }
        switch (endptr[0]) {
        case '\0':
            siv->unparsed_string = endptr;
            break;
        case ',':
            siv->unparsed_string = endptr + 1;
            break;
        default:
            return -EINVAL;
        }
        break;
    default:
        return -EINVAL;
    }

    siv->lm = LM_INT64_RANGE;
    siv->rangeNext.i64 = start;
    siv->rangeEnd.i64 = end;
    return 0;
}

static bool parse_type_int64(Visitor *v, const char *name, int64_t *obj,
                             Error **errp)
{
    StringInputVisitor *siv = to_siv(v);
    int64_t val;

    switch (siv->lm) {
    case LM_NONE:
        if (qemu_strtoi64(siv->string, NULL, 0, &val)) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE,
                       name ? name : "null", "int64");
            return false;
        }
        *obj = val;
        return true;
    case LM_UNPARSED:
        if (try_parse_int64_list_entry(siv, obj)) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE, name ? name : "null",
                       "list of int64 values or ranges");
            return false;
        }
        assert(siv->lm == LM_INT64_RANGE);
    case LM_INT64_RANGE:
        assert(siv->rangeNext.i64 <= siv->rangeEnd.i64);
        *obj = siv->rangeNext.i64++;

        if (siv->rangeNext.i64 > siv->rangeEnd.i64 || *obj == INT64_MAX) {
            siv->lm = siv->unparsed_string[0] ? LM_UNPARSED : LM_END;
        }
        return true;
    case LM_END:
        error_setg(errp, "Fewer list elements expected");
        return false;
    default:
        abort();
    }
}

static int try_parse_uint64_list_entry(StringInputVisitor *siv, uint64_t *obj)
{
    const char *endptr;
    uint64_t start, end;

    if (qemu_strtou64(siv->unparsed_string, &endptr, 0, &start)) {
        return -EINVAL;
    }
    end = start;

    switch (endptr[0]) {
    case '\0':
        siv->unparsed_string = endptr;
        break;
    case ',':
        siv->unparsed_string = endptr + 1;
        break;
    case '-':
        if (qemu_strtou64(endptr + 1, &endptr, 0, &end)) {
            return -EINVAL;
        }
        if (start > end || end - start >= RANGE_MAX_ELEMENTS) {
            return -EINVAL;
        }
        switch (endptr[0]) {
        case '\0':
            siv->unparsed_string = endptr;
            break;
        case ',':
            siv->unparsed_string = endptr + 1;
            break;
        default:
            return -EINVAL;
        }
        break;
    default:
        return -EINVAL;
    }

    siv->lm = LM_UINT64_RANGE;
    siv->rangeNext.u64 = start;
    siv->rangeEnd.u64 = end;
    return 0;
}

static bool parse_type_uint64(Visitor *v, const char *name, uint64_t *obj,
                              Error **errp)
{
    StringInputVisitor *siv = to_siv(v);
    uint64_t val;

    switch (siv->lm) {
    case LM_NONE:
        if (qemu_strtou64(siv->string, NULL, 0, &val)) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE, name ? name : "null",
                       "uint64");
            return false;
        }
        *obj = val;
        return true;
    case LM_UNPARSED:
        if (try_parse_uint64_list_entry(siv, obj)) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE, name ? name : "null",
                       "list of uint64 values or ranges");
            return false;
        }
        assert(siv->lm == LM_UINT64_RANGE);
    case LM_UINT64_RANGE:
        assert(siv->rangeNext.u64 <= siv->rangeEnd.u64);
        *obj = siv->rangeNext.u64++;

        if (siv->rangeNext.u64 > siv->rangeEnd.u64 || *obj == UINT64_MAX) {
            siv->lm = siv->unparsed_string[0] ? LM_UNPARSED : LM_END;
        }
        return true;
    case LM_END:
        error_setg(errp, "Fewer list elements expected");
        return false;
    default:
        abort();
    }
}

static bool parse_type_size(Visitor *v, const char *name, uint64_t *obj,
                            Error **errp)
{
    StringInputVisitor *siv = to_siv(v);
    uint64_t val;

    assert(siv->lm == LM_NONE);
    if (!parse_option_size(name, siv->string, &val, errp)) {
        return false;
    }

    *obj = val;
    return true;
}

static bool parse_type_bool(Visitor *v, const char *name, bool *obj,
                            Error **errp)
{
    StringInputVisitor *siv = to_siv(v);

    assert(siv->lm == LM_NONE);
    return qapi_bool_parse(name ? name : "null", siv->string, obj, errp);
}

static bool parse_type_str(Visitor *v, const char *name, char **obj,
                           Error **errp)
{
    StringInputVisitor *siv = to_siv(v);

    assert(siv->lm == LM_NONE);
    *obj = g_strdup(siv->string);
    return true;
}

static bool parse_type_number(Visitor *v, const char *name, double *obj,
                              Error **errp)
{
    StringInputVisitor *siv = to_siv(v);
    double val;

    assert(siv->lm == LM_NONE);
    if (qemu_strtod_finite(siv->string, NULL, &val)) {
        error_setg(errp, "Invalid parameter type for '%s', expected: number",
                   name ? name : "null");
        return false;
    }

    *obj = val;
    return true;
}

static bool parse_type_null(Visitor *v, const char *name, QNull **obj,
                            Error **errp)
{
    StringInputVisitor *siv = to_siv(v);

    assert(siv->lm == LM_NONE);
    *obj = NULL;

    if (siv->string[0]) {
        error_setg(errp, "Invalid parameter type for '%s', expected: null",
                   name ? name : "null");
        return false;
    }

    *obj = qnull();
    return true;
}

static void string_input_free(Visitor *v)
{
    StringInputVisitor *siv = to_siv(v);

    g_free(siv);
}

Visitor *string_input_visitor_new(const char *str)
{
    StringInputVisitor *v;

    assert(str);
    v = g_malloc0(sizeof(*v));

    v->visitor.type = VISITOR_INPUT;
    v->visitor.type_int64 = parse_type_int64;
    v->visitor.type_uint64 = parse_type_uint64;
    v->visitor.type_size = parse_type_size;
    v->visitor.type_bool = parse_type_bool;
    v->visitor.type_str = parse_type_str;
    v->visitor.type_number = parse_type_number;
    v->visitor.type_null = parse_type_null;
    v->visitor.start_list = start_list;
    v->visitor.next_list = next_list;
    v->visitor.check_list = check_list;
    v->visitor.end_list = end_list;
    v->visitor.free = string_input_free;

    v->string = str;
    v->lm = LM_NONE;
    return &v->visitor;
}
