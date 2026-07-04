
#ifndef QAPI_CLONE_VISITOR_H
#define QAPI_CLONE_VISITOR_H

#include "qapi/error.h"
#include "qapi/visitor.h"

typedef struct QapiCloneVisitor QapiCloneVisitor;

Visitor *qapi_clone_visitor_new(void);
Visitor *qapi_clone_members_visitor_new(void);

#define QAPI_CLONE(type, src)                                   \
    ({                                                          \
        Visitor *v_;                                            \
        type *dst_ = (type *) (src); /* Cast away const */      \
                                                                \
        if (dst_) {                                             \
            v_ = qapi_clone_visitor_new();                      \
            visit_type_ ## type(v_, NULL, &dst_, &error_abort); \
            visit_free(v_);                                     \
        }                                                       \
        dst_;                                                   \
    })

#define QAPI_CLONE_MEMBERS(type, dst, src)                                \
    ({                                                                    \
        Visitor *v_;                                                      \
                                                                          \
        v_ = qapi_clone_members_visitor_new();                            \
        *(type *)(dst) = *(src);                                          \
        visit_type_ ## type ## _members(v_, (type *)(dst), &error_abort); \
        visit_free(v_);                                                   \
    })

#endif
