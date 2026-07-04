
#ifndef STRING_OUTPUT_VISITOR_H
#define STRING_OUTPUT_VISITOR_H

#include "qapi/visitor.h"

typedef struct StringOutputVisitor StringOutputVisitor;

Visitor *string_output_visitor_new(bool human, char **result);

#endif
