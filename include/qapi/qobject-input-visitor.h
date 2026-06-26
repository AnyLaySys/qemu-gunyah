
#ifndef QOBJECT_INPUT_VISITOR_H
#define QOBJECT_INPUT_VISITOR_H

#include "qapi/visitor.h"

typedef struct QObjectInputVisitor QObjectInputVisitor;

Visitor *qobject_input_visitor_new(QObject *obj);

Visitor *qobject_input_visitor_new_keyval(QObject *obj);

Visitor *qobject_input_visitor_new_str(const char *str,
                                       const char *implied_key,
                                       Error **errp);

#endif
