#include "qemu/osdep.h"
#include "qapi/compat-policy.h"
#include "qapi/qobject-input-visitor.h"
#include "qapi/qobject-output-visitor.h"

Visitor *qobject_input_visitor_new_qmp(QObject *obj)
{
    Visitor *v = qobject_input_visitor_new(obj);

    visit_set_policy(v, &compat_policy);
    return v;
}

Visitor *qobject_output_visitor_new_qmp(QObject **result)
{
    Visitor *v = qobject_output_visitor_new(result);

    visit_set_policy(v, &compat_policy);
    return v;
}
