
#include "qemu/osdep.h"
#include "authz/simple.h"
#include "trace.h"
#include "qemu/module.h"
#include "qom/object_interfaces.h"

static bool qauthz_simple_is_allowed(QAuthZ *authz,
                                     const char *identity,
                                     Error **errp)
{
    QAuthZSimple *sauthz = QAUTHZ_SIMPLE(authz);

    trace_qauthz_simple_is_allowed(authz, sauthz->identity, identity);
    return g_str_equal(identity, sauthz->identity);
}

static void
qauthz_simple_prop_set_identity(Object *obj,
                                const char *value,
                                Error **errp G_GNUC_UNUSED)
{
    QAuthZSimple *sauthz = QAUTHZ_SIMPLE(obj);

    g_free(sauthz->identity);
    sauthz->identity = g_strdup(value);
}


static char *
qauthz_simple_prop_get_identity(Object *obj,
                                Error **errp G_GNUC_UNUSED)
{
    QAuthZSimple *sauthz = QAUTHZ_SIMPLE(obj);

    return g_strdup(sauthz->identity);
}


static void
qauthz_simple_finalize(Object *obj)
{
    QAuthZSimple *sauthz = QAUTHZ_SIMPLE(obj);

    g_free(sauthz->identity);
}


static void
qauthz_simple_complete(UserCreatable *uc, Error **errp)
{
    QAuthZSimple *sauthz = QAUTHZ_SIMPLE(uc);

    if (!sauthz->identity) {
        error_setg(errp, "The 'identity' property must be set");
        return;
    }
}


static void
qauthz_simple_class_init(ObjectClass *oc, void *data)
{
    QAuthZClass *authz = QAUTHZ_CLASS(oc);
    UserCreatableClass *ucc = USER_CREATABLE_CLASS(oc);

    ucc->complete = qauthz_simple_complete;
    authz->is_allowed = qauthz_simple_is_allowed;

    object_class_property_add_str(oc, "identity",
                                  qauthz_simple_prop_get_identity,
                                  qauthz_simple_prop_set_identity);
}


QAuthZSimple *qauthz_simple_new(const char *id,
                                const char *identity,
                                Error **errp)
{
    return QAUTHZ_SIMPLE(
        object_new_with_props(TYPE_QAUTHZ_SIMPLE,
                              object_get_objects_root(),
                              id, errp,
                              "identity", identity,
                              NULL));
}


static const TypeInfo qauthz_simple_info = {
    .parent = TYPE_QAUTHZ,
    .name = TYPE_QAUTHZ_SIMPLE,
    .instance_size = sizeof(QAuthZSimple),
    .instance_finalize = qauthz_simple_finalize,
    .class_init = qauthz_simple_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_USER_CREATABLE },
        { }
    }
};


static void
qauthz_simple_register_types(void)
{
    type_register_static(&qauthz_simple_info);
}


type_init(qauthz_simple_register_types);
