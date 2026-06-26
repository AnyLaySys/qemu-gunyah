
#ifndef QEMU_QOM_QOBJECT_H
#define QEMU_QOM_QOBJECT_H

struct QObject *object_property_get_qobject(Object *obj, const char *name,
                                            struct Error **errp);

bool object_property_set_qobject(Object *obj,
                                 const char *name, struct QObject *value,
                                 struct Error **errp);

#endif
