#ifndef OBJECT_INTERFACES_H
#define OBJECT_INTERFACES_H

#include "qom/object.h"
#include "qapi/qapi-types-qom.h"
#include "qapi/visitor.h"

#define TYPE_USER_CREATABLE "user-creatable"

typedef struct UserCreatableClass UserCreatableClass;
DECLARE_CLASS_CHECKERS(UserCreatableClass, USER_CREATABLE,
                       TYPE_USER_CREATABLE)
#define USER_CREATABLE(obj) \
     INTERFACE_CHECK(UserCreatable, (obj), \
                     TYPE_USER_CREATABLE)

typedef struct UserCreatable UserCreatable;

struct UserCreatableClass {
    InterfaceClass parent_class;

    void (*complete)(UserCreatable *uc, Error **errp);
    bool (*can_be_deleted)(UserCreatable *uc);
};

bool user_creatable_complete(UserCreatable *uc, Error **errp);

bool user_creatable_can_be_deleted(UserCreatable *uc);

Object *user_creatable_add_type(const char *type, const char *id,
                                const QDict *qdict,
                                Visitor *v, Error **errp);

void user_creatable_add_qapi(ObjectOptions *options, Error **errp);

ObjectOptions *user_creatable_parse_str(const char *str, Error **errp);

bool user_creatable_add_from_str(const char *str, Error **errp);

void user_creatable_process_cmdline(const char *cmdline);

bool user_creatable_print_help(const char *type, QemuOpts *opts);

bool user_creatable_del(const char *id, Error **errp);

void user_creatable_cleanup(void);

#endif
