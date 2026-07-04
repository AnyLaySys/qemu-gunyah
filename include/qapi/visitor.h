
#ifndef QAPI_VISITOR_H
#define QAPI_VISITOR_H

#include "qapi/qapi-builtin-types.h"
#include "qapi/qapi-types-compat.h"



typedef struct GenericList {
    struct GenericList *next;
    char padding[];
} GenericList;

typedef struct GenericAlternate {
    QType type;
    char padding[];
} GenericAlternate;


void visit_complete(Visitor *v, void *opaque);

void visit_free(Visitor *v);



bool visit_start_struct(Visitor *v, const char *name, void **obj,
                        size_t size, Error **errp);

bool visit_check_struct(Visitor *v, Error **errp);

void visit_end_struct(Visitor *v, void **obj);



bool visit_start_list(Visitor *v, const char *name, GenericList **list,
                      size_t size, Error **errp);

GenericList *visit_next_list(Visitor *v, GenericList *tail, size_t size);

bool visit_check_list(Visitor *v, Error **errp);

void visit_end_list(Visitor *v, void **list);



bool visit_start_alternate(Visitor *v, const char *name,
                           GenericAlternate **obj, size_t size,
                           Error **errp);

void visit_end_alternate(Visitor *v, void **obj);



bool visit_optional(Visitor *v, const char *name, bool *present);

bool visit_policy_reject(Visitor *v, const char *name,
                         uint64_t features, Error **errp);

bool visit_policy_skip(Visitor *v, const char *name,
                       uint64_t features);

void visit_set_policy(Visitor *v, CompatPolicy *policy);

bool visit_type_enum(Visitor *v, const char *name, int *obj,
                     const QEnumLookup *lookup, Error **errp);

bool visit_is_input(Visitor *v);

bool visit_is_dealloc(Visitor *v);


bool visit_type_int(Visitor *v, const char *name, int64_t *obj, Error **errp);

bool visit_type_uint8(Visitor *v, const char *name, uint8_t *obj,
                      Error **errp);

bool visit_type_uint16(Visitor *v, const char *name, uint16_t *obj,
                       Error **errp);

bool visit_type_uint32(Visitor *v, const char *name, uint32_t *obj,
                       Error **errp);

bool visit_type_uint64(Visitor *v, const char *name, uint64_t *obj,
                       Error **errp);

bool visit_type_int8(Visitor *v, const char *name, int8_t *obj, Error **errp);

bool visit_type_int16(Visitor *v, const char *name, int16_t *obj,
                      Error **errp);

bool visit_type_int32(Visitor *v, const char *name, int32_t *obj,
                      Error **errp);

bool visit_type_int64(Visitor *v, const char *name, int64_t *obj,
                      Error **errp);

bool visit_type_size(Visitor *v, const char *name, uint64_t *obj,
                     Error **errp);

bool visit_type_bool(Visitor *v, const char *name, bool *obj, Error **errp);

bool visit_type_str(Visitor *v, const char *name, char **obj, Error **errp);

bool visit_type_number(Visitor *v, const char *name, double *obj,
                       Error **errp);

bool visit_type_any(Visitor *v, const char *name, QObject **obj, Error **errp);

bool visit_type_null(Visitor *v, const char *name, QNull **obj,
                     Error **errp);

#endif
