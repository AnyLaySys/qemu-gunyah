
#ifndef QAPI_UTIL_H
#define QAPI_UTIL_H

typedef enum {
    QAPI_DEPRECATED,
    QAPI_UNSTABLE,
} QapiSpecialFeature;

typedef struct QEnumLookup {
    const char *const *array;
    const uint64_t *const features;
    const int size;
} QEnumLookup;

const char *qapi_enum_lookup(const QEnumLookup *lookup, int val);
int qapi_enum_parse(const QEnumLookup *lookup, const char *buf,
                    int def, Error **errp);
bool qapi_bool_parse(const char *name, const char *value, bool *obj,
                     Error **errp);

int parse_qapi_name(const char *name, bool complete);

#define QAPI_LIST_PREPEND(list, element) do { \
    typeof(list) _tmp = g_malloc(sizeof(*(list))); \
    _tmp->value = (element); \
    _tmp->next = (list); \
    (list) = _tmp; \
} while (0)

#define QAPI_LIST_APPEND(tail, element) do { \
    *(tail) = g_malloc0(sizeof(**(tail))); \
    (*(tail))->value = (element); \
    (tail) = &(*(tail))->next; \
} while (0)

#define QAPI_LIST_LENGTH(list)                                      \
    ({                                                              \
        size_t _len = 0;                                            \
        typeof_strip_qual(list) _tail;                              \
        for (_tail = list; _tail != NULL; _tail = _tail->next) {    \
            _len++;                                                 \
        }                                                           \
        _len;                                                       \
    })

#endif
