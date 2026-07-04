
#ifndef FORWARD_VISITOR_H
#define FORWARD_VISITOR_H

#include "qapi/visitor.h"

typedef struct ForwardFieldVisitor ForwardFieldVisitor;

Visitor *visitor_forward_field(Visitor *target, const char *from, const char *to);

#endif
