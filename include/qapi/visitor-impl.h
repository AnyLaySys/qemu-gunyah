#ifndef QAPI_VISITOR_IMPL_H
#define QAPI_VISITOR_IMPL_H

#include "qapi/visitor.h"


typedef enum VisitorType {
    VISITOR_INPUT = 1,
    VISITOR_OUTPUT = 2,
    VISITOR_CLONE = 3,
    VISITOR_DEALLOC = 4,
} VisitorType;

struct Visitor
{

    bool (*start_struct)(Visitor *v, const char *name, void **obj,
                         size_t size, Error **errp);

    bool (*check_struct)(Visitor *v, Error **errp);

    void (*end_struct)(Visitor *v, void **obj);

    bool (*start_list)(Visitor *v, const char *name, GenericList **list,
                       size_t size, Error **errp);

    GenericList *(*next_list)(Visitor *v, GenericList *tail, size_t size);

    bool (*check_list)(Visitor *v, Error **errp);

    void (*end_list)(Visitor *v, void **list);

    bool (*start_alternate)(Visitor *v, const char *name,
                            GenericAlternate **obj, size_t size,
                            Error **errp);

    void (*end_alternate)(Visitor *v, void **obj);

    bool (*type_int64)(Visitor *v, const char *name, int64_t *obj,
                       Error **errp);

    bool (*type_uint64)(Visitor *v, const char *name, uint64_t *obj,
                        Error **errp);

    bool (*type_size)(Visitor *v, const char *name, uint64_t *obj,
                      Error **errp);

    bool (*type_bool)(Visitor *v, const char *name, bool *obj, Error **errp);

    bool (*type_str)(Visitor *v, const char *name, char **obj, Error **errp);

    bool (*type_number)(Visitor *v, const char *name, double *obj,
                        Error **errp);

    bool (*type_any)(Visitor *v, const char *name, QObject **obj,
                     Error **errp);

    bool (*type_null)(Visitor *v, const char *name, QNull **obj,
                      Error **errp);

    void (*optional)(Visitor *v, const char *name, bool *present);

    bool (*policy_reject)(Visitor *v, const char *name,
                          uint64_t features, Error **errp);

    bool (*policy_skip)(Visitor *v, const char *name,
                        uint64_t features);

    VisitorType type;

    struct CompatPolicy compat_policy;

    void (*complete)(Visitor *v, void *opaque);

    void (*free)(Visitor *v);
};

#endif
