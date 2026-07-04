
#ifndef QAPI_DEALLOC_VISITOR_H
#define QAPI_DEALLOC_VISITOR_H

#include "qapi/visitor.h"

typedef struct QapiDeallocVisitor QapiDeallocVisitor;

Visitor *qapi_dealloc_visitor_new(void);

#endif
