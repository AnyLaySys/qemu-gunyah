
#ifndef OPTS_VISITOR_H
#define OPTS_VISITOR_H

#include "qapi/visitor.h"

#define OPTS_VISITOR_RANGE_MAX 65536

typedef struct OptsVisitor OptsVisitor;

Visitor *opts_visitor_new(const QemuOpts *opts);

#endif
