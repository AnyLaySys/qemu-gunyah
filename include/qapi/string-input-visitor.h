
#ifndef STRING_INPUT_VISITOR_H
#define STRING_INPUT_VISITOR_H

#include "qapi/visitor.h"

typedef struct StringInputVisitor StringInputVisitor;

Visitor *string_input_visitor_new(const char *str);

#endif
