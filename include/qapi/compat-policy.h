
#ifndef QAPI_COMPAT_POLICY_H
#define QAPI_COMPAT_POLICY_H

#include "qapi/error.h"
#include "qapi/qapi-types-compat.h"

extern CompatPolicy compat_policy;

bool compat_policy_input_ok(uint64_t features,
                            const CompatPolicy *policy,
                            ErrorClass error_class,
                            const char *kind, const char *name,
                            Error **errp);

Visitor *qobject_input_visitor_new_qmp(QObject *obj);

Visitor *qobject_output_visitor_new_qmp(QObject **result);

#endif
