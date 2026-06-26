
#include "qemu/osdep.h"
#include "qom/object.h"
#include "qemu/module.h"

static const TypeInfo container_info = {
    .name          = TYPE_CONTAINER,
    .parent        = TYPE_OBJECT,
};

static void container_register_types(void)
{
    type_register_static(&container_info);
}

Object *object_property_add_new_container(Object *obj, const char *name)
{
    Object *child = object_new(TYPE_CONTAINER);

    object_property_add_child(obj, name, child);
    object_unref(child);

    return child;
}

type_init(container_register_types)
