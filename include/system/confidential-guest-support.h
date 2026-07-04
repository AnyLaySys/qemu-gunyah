#ifndef QEMU_CONFIDENTIAL_GUEST_SUPPORT_H
#define QEMU_CONFIDENTIAL_GUEST_SUPPORT_H

#ifdef CONFIG_USER_ONLY
#error Cannot include system/confidential-guest-support.h from user emulation
#endif

#include "qom/object.h"

#define TYPE_CONFIDENTIAL_GUEST_SUPPORT "confidential-guest-support"
OBJECT_DECLARE_TYPE(ConfidentialGuestSupport,
                    ConfidentialGuestSupportClass,
                    CONFIDENTIAL_GUEST_SUPPORT)


struct ConfidentialGuestSupport {
    Object parent;

    bool require_guest_memfd;

    bool ready;
};

typedef struct ConfidentialGuestSupportClass {
    ObjectClass parent;

    int (*kvm_init)(ConfidentialGuestSupport *cgs, Error **errp);
    int (*kvm_reset)(ConfidentialGuestSupport *cgs, Error **errp);
} ConfidentialGuestSupportClass;

static inline int confidential_guest_kvm_init(ConfidentialGuestSupport *cgs,
                                              Error **errp)
{
    ConfidentialGuestSupportClass *klass;

    klass = CONFIDENTIAL_GUEST_SUPPORT_GET_CLASS(cgs);
    if (klass->kvm_init) {
        return klass->kvm_init(cgs, errp);
    }

    return 0;
}

static inline int confidential_guest_kvm_reset(ConfidentialGuestSupport *cgs,
                                               Error **errp)
{
    ConfidentialGuestSupportClass *klass;

    klass = CONFIDENTIAL_GUEST_SUPPORT_GET_CLASS(cgs);
    if (klass->kvm_reset) {
        return klass->kvm_reset(cgs, errp);
    }

    return 0;
}

#endif /* QEMU_CONFIDENTIAL_GUEST_SUPPORT_H */
