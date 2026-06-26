
#ifndef QAUTHZ_LIST_H
#define QAUTHZ_LIST_H

#include "authz/base.h"
#include "qapi/qapi-types-authz.h"
#include "qom/object.h"

#define TYPE_QAUTHZ_LIST "authz-list"

OBJECT_DECLARE_SIMPLE_TYPE(QAuthZList,
                           QAUTHZ_LIST)



struct QAuthZList {
    QAuthZ parent_obj;

    QAuthZListPolicy policy;
    QAuthZListRuleList *rules;
};




QAuthZList *qauthz_list_new(const char *id,
                            QAuthZListPolicy policy,
                            Error **errp);

ssize_t qauthz_list_append_rule(QAuthZList *auth,
                                const char *match,
                                QAuthZListPolicy policy,
                                QAuthZListFormat format,
                                Error **errp);

ssize_t qauthz_list_insert_rule(QAuthZList *auth,
                                const char *match,
                                QAuthZListPolicy policy,
                                QAuthZListFormat format,
                                size_t index,
                                Error **errp);

ssize_t qauthz_list_delete_rule(QAuthZList *auth,
                                const char *match);


#endif /* QAUTHZ_LIST_H */
