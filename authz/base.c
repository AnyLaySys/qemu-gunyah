
#include "qemu/osdep.h"
#include "authz/base.h"
#include "qemu/module.h"
#include "trace.h"

bool qauthz_is_allowed(QAuthZ *authz,
                       const char *identity,
                       Error **errp)
{
    QAuthZClass *cls = QAUTHZ_GET_CLASS(authz);
    bool allowed;

    allowed = cls->is_allowed(authz, identity, errp);
    trace_qauthz_is_allowed(authz, identity, allowed);

    return allowed;
}


bool qauthz_is_allowed_by_id(const char *authzid,
                             const char *identity,
                             Error **errp)
{
    QAuthZ *authz;
    Object *obj;
    Object *container;

    container = object_get_objects_root();
    obj = object_resolve_path_component(container,
                                        authzid);
    if (!obj) {
        error_setg(errp, "Cannot find QAuthZ object ID %s",
                   authzid);
        return false;
    }

    if (!object_dynamic_cast(obj, TYPE_QAUTHZ)) {
        error_setg(errp, "Object '%s' is not a QAuthZ subclass",
                   authzid);
        return false;
    }

    authz = QAUTHZ(obj);

    return qauthz_is_allowed(authz, identity, errp);
}


static const TypeInfo authz_info = {
    .parent = TYPE_OBJECT,
    .name = TYPE_QAUTHZ,
    .instance_size = sizeof(QAuthZ),
    .class_size = sizeof(QAuthZClass),
    .abstract = true,
};

static void qauthz_register_types(void)
{
    type_register_static(&authz_info);
}

type_init(qauthz_register_types)

