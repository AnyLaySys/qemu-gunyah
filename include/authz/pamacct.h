
#ifndef QAUTHZ_PAMACCT_H
#define QAUTHZ_PAMACCT_H

#include "authz/base.h"
#include "qom/object.h"


#define TYPE_QAUTHZ_PAM "authz-pam"

OBJECT_DECLARE_SIMPLE_TYPE(QAuthZPAM,
                           QAUTHZ_PAM)



struct QAuthZPAM {
    QAuthZ parent_obj;

    char *service;
};




QAuthZPAM *qauthz_pam_new(const char *id,
                          const char *service,
                          Error **errp);

#endif /* QAUTHZ_PAMACCT_H */
