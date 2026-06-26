
#ifndef HW_RESETCONTAINER_H
#define HW_RESETCONTAINER_H


#include "qom/object.h"

#define TYPE_RESETTABLE_CONTAINER "resettable-container"
OBJECT_DECLARE_TYPE(ResettableContainer, ResettableContainerClass, RESETTABLE_CONTAINER)

void resettable_container_add(ResettableContainer *rc, Object *obj);

void resettable_container_remove(ResettableContainer *rc, Object *obj);

#endif
