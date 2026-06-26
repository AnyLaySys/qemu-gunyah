
#ifndef QAUTHZ_BASE_H
#define QAUTHZ_BASE_H

#include "qapi/error.h"
#include "qom/object.h"


#define TYPE_QAUTHZ "authz"

OBJECT_DECLARE_TYPE(QAuthZ, QAuthZClass,
                    QAUTHZ)



struct QAuthZ {
    Object parent_obj;
};


struct QAuthZClass {
    ObjectClass parent_class;

    bool (*is_allowed)(QAuthZ *authz,
                       const char *identity,
                       Error **errp);
};


bool qauthz_is_allowed(QAuthZ *authz,
                       const char *identity,
                       Error **errp);


bool qauthz_is_allowed_by_id(const char *authzid,
                             const char *identity,
                             Error **errp);

#endif /* QAUTHZ_BASE_H */
