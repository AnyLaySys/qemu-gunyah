
#ifndef QOBJECT_OUTPUT_VISITOR_H
#define QOBJECT_OUTPUT_VISITOR_H

#include "qapi/visitor.h"

typedef struct QObjectOutputVisitor QObjectOutputVisitor;

Visitor *qobject_output_visitor_new(QObject **result);

#endif
