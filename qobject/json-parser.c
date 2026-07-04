
#include "qemu/osdep.h"
#include "qemu/ctype.h"
#include "qemu/cutils.h"
#include "qemu/unicode.h"
#include "qapi/error.h"
#include "qobject/qbool.h"
#include "qobject/qdict.h"
#include "qobject/qlist.h"
#include "qobject/qnull.h"
#include "qobject/qnum.h"
#include "qobject/qstring.h"
#include "json-parser-int.h"

struct JSONToken {
    JSONTokenType type;
    int x;
    int y;
    char str[];
};

typedef struct JSONParserContext {
    Error *err;
    JSONToken *current;
    GQueue *buf;
    va_list *ap;
} JSONParserContext;

#define BUG_ON(cond) assert(!(cond))


static QObject *parse_value(JSONParserContext *ctxt);

static void G_GNUC_PRINTF(3, 4) parse_error(JSONParserContext *ctxt,
                                           JSONToken *token, const char *msg, ...)
{
    va_list ap;
    char message[1024];

    if (ctxt->err) {
        return;
    }
    va_start(ap, msg);
    vsnprintf(message, sizeof(message), msg, ap);
    va_end(ap);
    error_setg(&ctxt->err, "JSON parse error, %s", message);
}

static int cvt4hex(const char *s)
{
    int cp, i;

    cp = 0;
    for (i = 0; i < 4; i++) {
        if (!qemu_isxdigit(s[i])) {
            return -1;
        }
        cp <<= 4;
        if (s[i] >= '0' && s[i] <= '9') {
            cp |= s[i] - '0';
        } else if (s[i] >= 'a' && s[i] <= 'f') {
            cp |= 10 + s[i] - 'a';
        } else if (s[i] >= 'A' && s[i] <= 'F') {
            cp |= 10 + s[i] - 'A';
        } else {
            return -1;
        }
    }
    return cp;
}

static QString *parse_string(JSONParserContext *ctxt, JSONToken *token)
{
    const char *ptr = token->str;
    GString *str;
    char quote;
    const char *beg;
    int cp, trailing;
    char *end;
    ssize_t len;
    char utf8_buf[5];

    assert(*ptr == '"' || *ptr == '\'');
    quote = *ptr++;
    str = g_string_new(NULL);

    while (*ptr != quote) {
        assert(*ptr);
        switch (*ptr) {
        case '\\':
            beg = ptr++;
            switch (*ptr++) {
            case '"':
                g_string_append_c(str, '"');
                break;
            case '\'':
                g_string_append_c(str, '\'');
                break;
            case '\\':
                g_string_append_c(str, '\\');
                break;
            case '/':
                g_string_append_c(str, '/');
                break;
            case 'b':
                g_string_append_c(str, '\b');
                break;
            case 'f':
                g_string_append_c(str, '\f');
                break;
            case 'n':
                g_string_append_c(str, '\n');
                break;
            case 'r':
                g_string_append_c(str, '\r');
                break;
            case 't':
                g_string_append_c(str, '\t');
                break;
            case 'u':
                cp = cvt4hex(ptr);
                ptr += 4;

                if (cp >= 0xD800 && cp <= 0xDBFF
                    && ptr[0] == '\\' && ptr[1] == 'u') {
                    cp = 0x10000 + ((cp & 0x3FF) << 10);
                    trailing = cvt4hex(ptr + 2);
                    if (trailing >= 0xDC00 && trailing <= 0xDFFF) {
                        cp |= trailing & 0x3FF;
                        ptr += 6;
                    } else {
                        cp = -1; /* invalid */
                    }
                }

                if (mod_utf8_encode(utf8_buf, sizeof(utf8_buf), cp) < 0) {
                    parse_error(ctxt, token,
                                "%.*s is not a valid Unicode character",
                                (int)(ptr - beg), beg);
                    goto out;
                }
                g_string_append(str, utf8_buf);
                break;
            default:
                parse_error(ctxt, token, "invalid escape sequence in string");
                goto out;
            }
            break;
        case '%':
            if (ctxt->ap) {
                if (ptr[1] != '%') {
                    parse_error(ctxt, token, "can't interpolate into string");
                    goto out;
                }
                ptr++;
            }
        default:
            cp = mod_utf8_codepoint(ptr, 6, &end);
            if (cp < 0) {
                parse_error(ctxt, token, "invalid UTF-8 sequence in string");
                goto out;
            }
            ptr = end;
            len = mod_utf8_encode(utf8_buf, sizeof(utf8_buf), cp);
            assert(len >= 0);
            g_string_append(str, utf8_buf);
        }
    }

    return qstring_from_gstring(str);

out:
    g_string_free(str, true);
    return NULL;
}

static JSONToken *parser_context_pop_token(JSONParserContext *ctxt)
{
    g_free(ctxt->current);
    ctxt->current = g_queue_pop_head(ctxt->buf);
    return ctxt->current;
}

static JSONToken *parser_context_peek_token(JSONParserContext *ctxt)
{
    return g_queue_peek_head(ctxt->buf);
}

static int parse_pair(JSONParserContext *ctxt, QDict *dict)
{
    QObject *key_obj = NULL;
    QString *key;
    QObject *value;
    JSONToken *peek, *token;

    peek = parser_context_peek_token(ctxt);
    if (peek == NULL) {
        parse_error(ctxt, NULL, "premature EOI");
        goto out;
    }

    key_obj = parse_value(ctxt);
    key = qobject_to(QString, key_obj);
    if (!key) {
        parse_error(ctxt, peek, "key is not a string in object");
        goto out;
    }

    token = parser_context_pop_token(ctxt);
    if (token == NULL) {
        parse_error(ctxt, NULL, "premature EOI");
        goto out;
    }

    if (token->type != JSON_COLON) {
        parse_error(ctxt, token, "missing : in object pair");
        goto out;
    }

    value = parse_value(ctxt);
    if (value == NULL) {
        parse_error(ctxt, token, "Missing value in dict");
        goto out;
    }

    if (qdict_haskey(dict, qstring_get_str(key))) {
        parse_error(ctxt, token, "duplicate key");
        goto out;
    }

    qdict_put_obj(dict, qstring_get_str(key), value);

    qobject_unref(key_obj);
    return 0;

out:
    qobject_unref(key_obj);
    return -1;
}

static QObject *parse_object(JSONParserContext *ctxt)
{
    QDict *dict = NULL;
    JSONToken *token, *peek;

    token = parser_context_pop_token(ctxt);
    assert(token && token->type == JSON_LCURLY);

    dict = qdict_new();

    peek = parser_context_peek_token(ctxt);
    if (peek == NULL) {
        parse_error(ctxt, NULL, "premature EOI");
        goto out;
    }

    if (peek->type != JSON_RCURLY) {
        if (parse_pair(ctxt, dict) == -1) {
            goto out;
        }

        token = parser_context_pop_token(ctxt);
        if (token == NULL) {
            parse_error(ctxt, NULL, "premature EOI");
            goto out;
        }

        while (token->type != JSON_RCURLY) {
            if (token->type != JSON_COMMA) {
                parse_error(ctxt, token, "expected separator in dict");
                goto out;
            }

            if (parse_pair(ctxt, dict) == -1) {
                goto out;
            }

            token = parser_context_pop_token(ctxt);
            if (token == NULL) {
                parse_error(ctxt, NULL, "premature EOI");
                goto out;
            }
        }
    } else {
        (void)parser_context_pop_token(ctxt);
    }

    return QOBJECT(dict);

out:
    qobject_unref(dict);
    return NULL;
}

static QObject *parse_array(JSONParserContext *ctxt)
{
    QList *list = NULL;
    JSONToken *token, *peek;

    token = parser_context_pop_token(ctxt);
    assert(token && token->type == JSON_LSQUARE);

    list = qlist_new();

    peek = parser_context_peek_token(ctxt);
    if (peek == NULL) {
        parse_error(ctxt, NULL, "premature EOI");
        goto out;
    }

    if (peek->type != JSON_RSQUARE) {
        QObject *obj;

        obj = parse_value(ctxt);
        if (obj == NULL) {
            parse_error(ctxt, token, "expecting value");
            goto out;
        }

        qlist_append_obj(list, obj);

        token = parser_context_pop_token(ctxt);
        if (token == NULL) {
            parse_error(ctxt, NULL, "premature EOI");
            goto out;
        }

        while (token->type != JSON_RSQUARE) {
            if (token->type != JSON_COMMA) {
                parse_error(ctxt, token, "expected separator in list");
                goto out;
            }

            obj = parse_value(ctxt);
            if (obj == NULL) {
                parse_error(ctxt, token, "expecting value");
                goto out;
            }

            qlist_append_obj(list, obj);

            token = parser_context_pop_token(ctxt);
            if (token == NULL) {
                parse_error(ctxt, NULL, "premature EOI");
                goto out;
            }
        }
    } else {
        (void)parser_context_pop_token(ctxt);
    }

    return QOBJECT(list);

out:
    qobject_unref(list);
    return NULL;
}

static QObject *parse_keyword(JSONParserContext *ctxt)
{
    JSONToken *token;

    token = parser_context_pop_token(ctxt);
    assert(token && token->type == JSON_KEYWORD);

    if (!strcmp(token->str, "true")) {
        return QOBJECT(qbool_from_bool(true));
    } else if (!strcmp(token->str, "false")) {
        return QOBJECT(qbool_from_bool(false));
    } else if (!strcmp(token->str, "null")) {
        return QOBJECT(qnull());
    }
    parse_error(ctxt, token, "invalid keyword '%s'", token->str);
    return NULL;
}

static QObject *parse_interpolation(JSONParserContext *ctxt)
{
    JSONToken *token;

    token = parser_context_pop_token(ctxt);
    assert(token && token->type == JSON_INTERP);

    if (!strcmp(token->str, "%p")) {
        return va_arg(*ctxt->ap, QObject *);
    } else if (!strcmp(token->str, "%i")) {
        return QOBJECT(qbool_from_bool(va_arg(*ctxt->ap, int)));
    } else if (!strcmp(token->str, "%d")) {
        return QOBJECT(qnum_from_int(va_arg(*ctxt->ap, int)));
    } else if (!strcmp(token->str, "%ld")) {
        return QOBJECT(qnum_from_int(va_arg(*ctxt->ap, long)));
    } else if (!strcmp(token->str, "%lld")) {
        return QOBJECT(qnum_from_int(va_arg(*ctxt->ap, long long)));
    } else if (!strcmp(token->str, "%" PRId64)) {
        return QOBJECT(qnum_from_int(va_arg(*ctxt->ap, int64_t)));
    } else if (!strcmp(token->str, "%u")) {
        return QOBJECT(qnum_from_uint(va_arg(*ctxt->ap, unsigned int)));
    } else if (!strcmp(token->str, "%lu")) {
        return QOBJECT(qnum_from_uint(va_arg(*ctxt->ap, unsigned long)));
    } else if (!strcmp(token->str, "%llu")) {
        return QOBJECT(qnum_from_uint(va_arg(*ctxt->ap, unsigned long long)));
    } else if (!strcmp(token->str, "%" PRIu64)) {
        return QOBJECT(qnum_from_uint(va_arg(*ctxt->ap, uint64_t)));
    } else if (!strcmp(token->str, "%s")) {
        return QOBJECT(qstring_from_str(va_arg(*ctxt->ap, const char *)));
    } else if (!strcmp(token->str, "%f")) {
        return QOBJECT(qnum_from_double(va_arg(*ctxt->ap, double)));
    }
    parse_error(ctxt, token, "invalid interpolation '%s'", token->str);
    return NULL;
}

static QObject *parse_literal(JSONParserContext *ctxt)
{
    JSONToken *token;

    token = parser_context_pop_token(ctxt);
    assert(token);

    switch (token->type) {
    case JSON_STRING:
        return QOBJECT(parse_string(ctxt, token));
    case JSON_INTEGER: {
        int ret;
        int64_t value;
        uint64_t uvalue;

        ret = qemu_strtoi64(token->str, NULL, 10, &value);
        if (!ret) {
            return QOBJECT(qnum_from_int(value));
        }
        assert(ret == -ERANGE);

        if (token->str[0] != '-') {
            ret = qemu_strtou64(token->str, NULL, 10, &uvalue);
            if (!ret) {
                return QOBJECT(qnum_from_uint(uvalue));
            }
            assert(ret == -ERANGE);
        }
    }
    case JSON_FLOAT:
        return QOBJECT(qnum_from_double(strtod(token->str, NULL)));
    default:
        abort();
    }
}

static QObject *parse_value(JSONParserContext *ctxt)
{
    JSONToken *token;

    token = parser_context_peek_token(ctxt);
    if (token == NULL) {
        parse_error(ctxt, NULL, "premature EOI");
        return NULL;
    }

    switch (token->type) {
    case JSON_LCURLY:
        return parse_object(ctxt);
    case JSON_LSQUARE:
        return parse_array(ctxt);
    case JSON_INTERP:
        return parse_interpolation(ctxt);
    case JSON_INTEGER:
    case JSON_FLOAT:
    case JSON_STRING:
        return parse_literal(ctxt);
    case JSON_KEYWORD:
        return parse_keyword(ctxt);
    default:
        parse_error(ctxt, token, "expecting value");
        return NULL;
    }
}

JSONToken *json_token(JSONTokenType type, int x, int y, GString *tokstr)
{
    JSONToken *token = g_malloc(sizeof(JSONToken) + tokstr->len + 1);

    token->type = type;
    memcpy(token->str, tokstr->str, tokstr->len);
    token->str[tokstr->len] = 0;
    token->x = x;
    token->y = y;
    return token;
}

QObject *json_parser_parse(GQueue *tokens, va_list *ap, Error **errp)
{
    JSONParserContext ctxt = { .buf = tokens, .ap = ap };
    QObject *result;

    result = parse_value(&ctxt);
    assert(ctxt.err || g_queue_is_empty(ctxt.buf));

    error_propagate(errp, ctxt.err);

    while (!g_queue_is_empty(ctxt.buf)) {
        parser_context_pop_token(&ctxt);
    }
    g_free(ctxt.current);

    return result;
}
