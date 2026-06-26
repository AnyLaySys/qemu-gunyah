
#ifndef QAUTHZ_SIMPLE_H
#define QAUTHZ_SIMPLE_H

#include "authz/base.h"
#include "qom/object.h"

#define TYPE_QAUTHZ_SIMPLE "authz-simple"

OBJECT_DECLARE_SIMPLE_TYPE(QAuthZSimple,
                           QAUTHZ_SIMPLE)



struct QAuthZSimple {
    QAuthZ parent_obj;

    char *identity;
};




QAuthZSimple *qauthz_simple_new(const char *id,
                                const char *identity,
                                Error **errp);


#endif /* QAUTHZ_SIMPLE_H */
