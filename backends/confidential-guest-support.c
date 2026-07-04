
#include "qemu/osdep.h"

#include "system/confidential-guest-support.h"

OBJECT_DEFINE_ABSTRACT_TYPE(ConfidentialGuestSupport,
                            confidential_guest_support,
                            CONFIDENTIAL_GUEST_SUPPORT,
                            OBJECT)

static void confidential_guest_support_class_init(ObjectClass *oc, void *data)
{
}

static void confidential_guest_support_init(Object *obj)
{
}

static void confidential_guest_support_finalize(Object *obj)
{
}
