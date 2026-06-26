
#ifndef QEMU_OBJECT_H
#define QEMU_OBJECT_H

#include "qapi/qapi-builtin-types.h"
#include "qemu/module.h"

struct TypeImpl;
typedef struct TypeImpl *Type;

typedef struct TypeInfo TypeInfo;

typedef struct InterfaceClass InterfaceClass;
typedef struct InterfaceInfo InterfaceInfo;

#define TYPE_OBJECT "object"
#define TYPE_CONTAINER "container"

typedef struct ObjectProperty ObjectProperty;

typedef void (ObjectPropertyAccessor)(Object *obj,
                                      Visitor *v,
                                      const char *name,
                                      void *opaque,
                                      Error **errp);

typedef Object *(ObjectPropertyResolve)(Object *obj,
                                        void *opaque,
                                        const char *part);

typedef void (ObjectPropertyRelease)(Object *obj,
                                     const char *name,
                                     void *opaque);

typedef void (ObjectPropertyInit)(Object *obj, ObjectProperty *prop);

struct ObjectProperty
{
    char *name;
    char *type;
    char *description;
    ObjectPropertyAccessor *get;
    ObjectPropertyAccessor *set;
    ObjectPropertyResolve *resolve;
    ObjectPropertyRelease *release;
    ObjectPropertyInit *init;
    void *opaque;
    QObject *defval;
};

typedef void (ObjectUnparent)(Object *obj);

typedef void (ObjectFree)(void *obj);

#define OBJECT_CLASS_CAST_CACHE 4

struct ObjectClass
{
    Type type;
    GSList *interfaces;

    const char *object_cast_cache[OBJECT_CLASS_CAST_CACHE];
    const char *class_cast_cache[OBJECT_CLASS_CAST_CACHE];

    ObjectUnparent *unparent;

    GHashTable *properties;
};

struct Object
{
    ObjectClass *class;
    ObjectFree *free;
    GHashTable *properties;
    uint32_t ref;
    Object *parent;
};

#define DECLARE_INSTANCE_CHECKER(InstanceType, OBJ_NAME, TYPENAME) \
    static inline G_GNUC_UNUSED InstanceType * \
    OBJ_NAME(const void *obj) \
    { return OBJECT_CHECK(InstanceType, obj, TYPENAME); }

#define DECLARE_CLASS_CHECKERS(ClassType, OBJ_NAME, TYPENAME) \
    static inline G_GNUC_UNUSED ClassType * \
    OBJ_NAME##_GET_CLASS(const void *obj) \
    { return OBJECT_GET_CLASS(ClassType, obj, TYPENAME); } \
    \
    static inline G_GNUC_UNUSED ClassType * \
    OBJ_NAME##_CLASS(const void *klass) \
    { return OBJECT_CLASS_CHECK(ClassType, klass, TYPENAME); }

#define DECLARE_OBJ_CHECKERS(InstanceType, ClassType, OBJ_NAME, TYPENAME) \
    DECLARE_INSTANCE_CHECKER(InstanceType, OBJ_NAME, TYPENAME) \
    \
    DECLARE_CLASS_CHECKERS(ClassType, OBJ_NAME, TYPENAME)

#define OBJECT_DECLARE_TYPE(InstanceType, ClassType, MODULE_OBJ_NAME) \
    typedef struct InstanceType InstanceType; \
    typedef struct ClassType ClassType; \
    \
    G_DEFINE_AUTOPTR_CLEANUP_FUNC(InstanceType, object_unref) \
    \
    DECLARE_OBJ_CHECKERS(InstanceType, ClassType, \
                         MODULE_OBJ_NAME, TYPE_##MODULE_OBJ_NAME)

#define OBJECT_DECLARE_SIMPLE_TYPE(InstanceType, MODULE_OBJ_NAME) \
    typedef struct InstanceType InstanceType; \
    \
    G_DEFINE_AUTOPTR_CLEANUP_FUNC(InstanceType, object_unref) \
    \
    DECLARE_INSTANCE_CHECKER(InstanceType, MODULE_OBJ_NAME, TYPE_##MODULE_OBJ_NAME)


#define DO_OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                       MODULE_OBJ_NAME, \
                                       PARENT_MODULE_OBJ_NAME, \
                                       ABSTRACT, CLASS_SIZE, ...) \
    static void \
    module_obj_name##_finalize(Object *obj); \
    static void \
    module_obj_name##_class_init(ObjectClass *oc, void *data); \
    static void \
    module_obj_name##_init(Object *obj); \
    \
    static const TypeInfo module_obj_name##_info = { \
        .parent = TYPE_##PARENT_MODULE_OBJ_NAME, \
        .name = TYPE_##MODULE_OBJ_NAME, \
        .instance_size = sizeof(ModuleObjName), \
        .instance_align = __alignof__(ModuleObjName), \
        .instance_init = module_obj_name##_init, \
        .instance_finalize = module_obj_name##_finalize, \
        .class_size = CLASS_SIZE, \
        .class_init = module_obj_name##_class_init, \
        .abstract = ABSTRACT, \
        .interfaces = (InterfaceInfo[]) { __VA_ARGS__ } , \
    }; \
    \
    static void \
    module_obj_name##_register_types(void) \
    { \
        type_register_static(&module_obj_name##_info); \
    } \
    type_init(module_obj_name##_register_types);

#define OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                    MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                    ABSTRACT, ...) \
    DO_OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                   MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                   ABSTRACT, sizeof(ModuleObjName##Class), \
                                   __VA_ARGS__)

#define OBJECT_DEFINE_TYPE(ModuleObjName, module_obj_name, MODULE_OBJ_NAME, \
                           PARENT_MODULE_OBJ_NAME) \
    OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                false, { NULL })

#define OBJECT_DEFINE_TYPE_WITH_INTERFACES(ModuleObjName, module_obj_name, \
                                           MODULE_OBJ_NAME, \
                                           PARENT_MODULE_OBJ_NAME, ...) \
    OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                false, __VA_ARGS__)

#define OBJECT_DEFINE_ABSTRACT_TYPE(ModuleObjName, module_obj_name, \
                                    MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME) \
    OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                true, { NULL })

#define OBJECT_DEFINE_SIMPLE_TYPE_WITH_INTERFACES(ModuleObjName, \
                                                  module_obj_name, \
                                                  MODULE_OBJ_NAME, \
                                                  PARENT_MODULE_OBJ_NAME, ...) \
    DO_OBJECT_DEFINE_TYPE_EXTENDED(ModuleObjName, module_obj_name, \
                                   MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, \
                                   false, 0, __VA_ARGS__)

#define OBJECT_DEFINE_SIMPLE_TYPE(ModuleObjName, module_obj_name, \
                                  MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME) \
    OBJECT_DEFINE_SIMPLE_TYPE_WITH_INTERFACES(ModuleObjName, module_obj_name, \
        MODULE_OBJ_NAME, PARENT_MODULE_OBJ_NAME, { NULL })

struct TypeInfo
{
    const char *name;
    const char *parent;

    size_t instance_size;
    size_t instance_align;
    void (*instance_init)(Object *obj);
    void (*instance_post_init)(Object *obj);
    void (*instance_finalize)(Object *obj);

    bool abstract;
    size_t class_size;

    void (*class_init)(ObjectClass *klass, void *data);
    void (*class_base_init)(ObjectClass *klass, void *data);
    void *class_data;

    InterfaceInfo *interfaces;
};

#define OBJECT(obj) \
    ((Object *)(obj))

#define OBJECT_CLASS(class) \
    ((ObjectClass *)(class))

#define OBJECT_CHECK(type, obj, name) \
    ((type *)object_dynamic_cast_assert(OBJECT(obj), (name), \
                                        __FILE__, __LINE__, __func__))

#define OBJECT_CLASS_CHECK(class_type, class, name) \
    ((class_type *)object_class_dynamic_cast_assert(OBJECT_CLASS(class), (name), \
                                               __FILE__, __LINE__, __func__))

#define OBJECT_GET_CLASS(class, obj, name) \
    OBJECT_CLASS_CHECK(class, object_get_class(OBJECT(obj)), name)

struct InterfaceInfo {
    const char *type;
};

struct InterfaceClass
{
    ObjectClass parent_class;
    Type interface_type;
};

#define TYPE_INTERFACE "interface"

#define INTERFACE_CLASS(klass) \
    OBJECT_CLASS_CHECK(InterfaceClass, klass, TYPE_INTERFACE)

#define INTERFACE_CHECK(interface, obj, name) \
    ((interface *)object_dynamic_cast_assert(OBJECT((obj)), (name), \
                                             __FILE__, __LINE__, __func__))

Object *object_new_with_class(ObjectClass *klass);

Object *object_new(const char *typename);

Object *object_new_with_props(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              ...) G_GNUC_NULL_TERMINATED;

Object *object_new_with_propv(const char *typename,
                              Object *parent,
                              const char *id,
                              Error **errp,
                              va_list vargs);

bool object_apply_global_props(Object *obj, const GPtrArray *props,
                               Error **errp);
void object_set_machine_compat_props(GPtrArray *compat_props);
void object_set_accelerator_compat_props(GPtrArray *compat_props);
void object_register_sugar_prop(const char *driver, const char *prop,
                                const char *value, bool optional);
void object_apply_compat_props(Object *obj);

bool object_set_props(Object *obj, Error **errp, ...) G_GNUC_NULL_TERMINATED;

bool object_set_propv(Object *obj, Error **errp, va_list vargs);

void object_initialize(void *obj, size_t size, const char *typename);

bool object_initialize_child_with_props(Object *parentobj,
                             const char *propname,
                             void *childobj, size_t size, const char *type,
                             Error **errp, ...) G_GNUC_NULL_TERMINATED;

bool object_initialize_child_with_propsv(Object *parentobj,
                              const char *propname,
                              void *childobj, size_t size, const char *type,
                              Error **errp, va_list vargs);

#define object_initialize_child(parent, propname, child, type)          \
    object_initialize_child_internal((parent), (propname),              \
                                     (child), sizeof(*(child)), (type))
void object_initialize_child_internal(Object *parent, const char *propname,
                                      void *child, size_t size,
                                      const char *type);

Object *object_dynamic_cast(Object *obj, const char *typename);

Object *object_dynamic_cast_assert(Object *obj, const char *typename,
                                   const char *file, int line, const char *func);

ObjectClass *object_get_class(Object *obj);

const char *object_get_typename(const Object *obj);

Type type_register_static(const TypeInfo *info);

void type_register_static_array(const TypeInfo *infos, int nr_infos);

#define DEFINE_TYPES(type_array)                                            \
static void do_qemu_init_ ## type_array(void)                               \
{                                                                           \
    type_register_static_array(type_array, ARRAY_SIZE(type_array));         \
}                                                                           \
type_init(do_qemu_init_ ## type_array)

bool type_print_class_properties(const char *type);

void object_set_properties_from_keyval(Object *obj, const QDict *qdict,
                                       bool from_json, Error **errp);

ObjectClass *object_class_dynamic_cast_assert(ObjectClass *klass,
                                              const char *typename,
                                              const char *file, int line,
                                              const char *func);

ObjectClass *object_class_dynamic_cast(ObjectClass *klass,
                                       const char *typename);

ObjectClass *object_class_get_parent(ObjectClass *klass);

const char *object_class_get_name(ObjectClass *klass);

bool object_class_is_abstract(ObjectClass *klass);

ObjectClass *object_class_by_name(const char *typename);

ObjectClass *module_object_class_by_name(const char *typename);

void object_class_foreach(void (*fn)(ObjectClass *klass, void *opaque),
                          const char *implements_type, bool include_abstract,
                          void *opaque);

GSList *object_class_get_list(const char *implements_type,
                              bool include_abstract);

GSList *object_class_get_list_sorted(const char *implements_type,
                              bool include_abstract);

Object *object_ref(void *obj);

void object_unref(void *obj);

ObjectProperty *object_property_try_add(Object *obj, const char *name,
                                        const char *type,
                                        ObjectPropertyAccessor *get,
                                        ObjectPropertyAccessor *set,
                                        ObjectPropertyRelease *release,
                                        void *opaque, Error **errp);

ObjectProperty *object_property_add(Object *obj, const char *name,
                                    const char *type,
                                    ObjectPropertyAccessor *get,
                                    ObjectPropertyAccessor *set,
                                    ObjectPropertyRelease *release,
                                    void *opaque);

void object_property_del(Object *obj, const char *name);

ObjectProperty *object_class_property_add(ObjectClass *klass, const char *name,
                                          const char *type,
                                          ObjectPropertyAccessor *get,
                                          ObjectPropertyAccessor *set,
                                          ObjectPropertyRelease *release,
                                          void *opaque);

void object_property_set_default_bool(ObjectProperty *prop, bool value);

void object_property_set_default_str(ObjectProperty *prop, const char *value);

void object_property_set_default_list(ObjectProperty *prop);

void object_property_set_default_int(ObjectProperty *prop, int64_t value);

void object_property_set_default_uint(ObjectProperty *prop, uint64_t value);

ObjectProperty *object_property_find(Object *obj, const char *name);

ObjectProperty *object_property_find_err(Object *obj,
                                         const char *name,
                                         Error **errp);

ObjectProperty *object_class_property_find(ObjectClass *klass,
                                           const char *name);

ObjectProperty *object_class_property_find_err(ObjectClass *klass,
                                               const char *name,
                                               Error **errp);

typedef struct ObjectPropertyIterator {
    ObjectClass *nextclass;
    GHashTableIter iter;
} ObjectPropertyIterator;

void object_property_iter_init(ObjectPropertyIterator *iter,
                               Object *obj);

void object_class_property_iter_init(ObjectPropertyIterator *iter,
                                     ObjectClass *klass);

ObjectProperty *object_property_iter_next(ObjectPropertyIterator *iter);

void object_unparent(Object *obj);

bool object_property_get(Object *obj, const char *name, Visitor *v,
                         Error **errp);

bool object_property_set_str(Object *obj, const char *name,
                             const char *value, Error **errp);

char *object_property_get_str(Object *obj, const char *name,
                              Error **errp);

bool object_property_set_link(Object *obj, const char *name,
                              Object *value, Error **errp);

Object *object_property_get_link(Object *obj, const char *name,
                                 Error **errp);

bool object_property_set_bool(Object *obj, const char *name,
                              bool value, Error **errp);

bool object_property_get_bool(Object *obj, const char *name,
                              Error **errp);

bool object_property_set_int(Object *obj, const char *name,
                             int64_t value, Error **errp);

int64_t object_property_get_int(Object *obj, const char *name,
                                Error **errp);

bool object_property_set_uint(Object *obj, const char *name,
                              uint64_t value, Error **errp);

uint64_t object_property_get_uint(Object *obj, const char *name,
                                  Error **errp);

int object_property_get_enum(Object *obj, const char *name,
                             const char *typename, Error **errp);

bool object_property_set(Object *obj, const char *name, Visitor *v,
                         Error **errp);

bool object_property_parse(Object *obj, const char *name,
                           const char *string, Error **errp);

char *object_property_print(Object *obj, const char *name, bool human,
                            Error **errp);

const char *object_property_get_type(Object *obj, const char *name,
                                     Error **errp);

Object *object_get_root(void);

Object *object_get_container(const char *name);


Object *object_get_objects_root(void);

Object *object_get_internal_root(void);

const char *object_get_canonical_path_component(const Object *obj);

char *object_get_canonical_path(const Object *obj);

Object *object_resolve_path(const char *path, bool *ambiguous);

Object *object_resolve_path_type(const char *path, const char *typename,
                                 bool *ambiguous);

Object *object_resolve_type_unambiguous(const char *typename, Error **errp);

Object *object_resolve_path_at(Object *parent, const char *path);

Object *object_resolve_path_component(Object *parent, const char *part);

ObjectProperty *object_property_try_add_child(Object *obj, const char *name,
                                              Object *child, Error **errp);

ObjectProperty *object_property_add_child(Object *obj, const char *name,
                                          Object *child);

typedef enum {
    OBJ_PROP_LINK_STRONG = 0x1,

    OBJ_PROP_LINK_DIRECT = 0x2,
    OBJ_PROP_LINK_CLASS = 0x4,
} ObjectPropertyLinkFlags;

void object_property_allow_set_link(const Object *obj, const char *name,
                                    Object *child, Error **errp);

ObjectProperty *object_property_add_link(Object *obj, const char *name,
                              const char *type, Object **targetp,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);

ObjectProperty *object_class_property_add_link(ObjectClass *oc,
                              const char *name,
                              const char *type, ptrdiff_t offset,
                              void (*check)(const Object *obj, const char *name,
                                            Object *val, Error **errp),
                              ObjectPropertyLinkFlags flags);

ObjectProperty *object_property_add_str(Object *obj, const char *name,
                             char *(*get)(Object *, Error **),
                             void (*set)(Object *, const char *, Error **));

ObjectProperty *object_class_property_add_str(ObjectClass *klass,
                                   const char *name,
                                   char *(*get)(Object *, Error **),
                                   void (*set)(Object *, const char *,
                                               Error **));

ObjectProperty *object_property_add_bool(Object *obj, const char *name,
                              bool (*get)(Object *, Error **),
                              void (*set)(Object *, bool, Error **));

ObjectProperty *object_class_property_add_bool(ObjectClass *klass,
                                    const char *name,
                                    bool (*get)(Object *, Error **),
                                    void (*set)(Object *, bool, Error **));

ObjectProperty *object_property_add_enum(Object *obj, const char *name,
                              const char *typename,
                              const QEnumLookup *lookup,
                              int (*get)(Object *, Error **),
                              void (*set)(Object *, int, Error **));

ObjectProperty *object_class_property_add_enum(ObjectClass *klass,
                                    const char *name,
                                    const char *typename,
                                    const QEnumLookup *lookup,
                                    int (*get)(Object *, Error **),
                                    void (*set)(Object *, int, Error **));

ObjectProperty *object_property_add_tm(Object *obj, const char *name,
                            void (*get)(Object *, struct tm *, Error **));

ObjectProperty *object_class_property_add_tm(ObjectClass *klass,
                            const char *name,
                            void (*get)(Object *, struct tm *, Error **));

typedef enum {
    OBJ_PROP_FLAG_READ = 1 << 0,
    OBJ_PROP_FLAG_WRITE = 1 << 1,
    OBJ_PROP_FLAG_READWRITE = (OBJ_PROP_FLAG_READ | OBJ_PROP_FLAG_WRITE),
} ObjectPropertyFlags;

ObjectProperty *object_property_add_uint8_ptr(Object *obj, const char *name,
                                              const uint8_t *v,
                                              ObjectPropertyFlags flags);

ObjectProperty *object_class_property_add_uint8_ptr(ObjectClass *klass,
                                         const char *name,
                                         const uint8_t *v,
                                         ObjectPropertyFlags flags);

ObjectProperty *object_property_add_uint16_ptr(Object *obj, const char *name,
                                    const uint16_t *v,
                                    ObjectPropertyFlags flags);

ObjectProperty *object_class_property_add_uint16_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint16_t *v,
                                          ObjectPropertyFlags flags);

ObjectProperty *object_property_add_uint32_ptr(Object *obj, const char *name,
                                    const uint32_t *v,
                                    ObjectPropertyFlags flags);

ObjectProperty *object_class_property_add_uint32_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint32_t *v,
                                          ObjectPropertyFlags flags);

ObjectProperty *object_property_add_uint64_ptr(Object *obj, const char *name,
                                    const uint64_t *v,
                                    ObjectPropertyFlags flags);

ObjectProperty *object_class_property_add_uint64_ptr(ObjectClass *klass,
                                          const char *name,
                                          const uint64_t *v,
                                          ObjectPropertyFlags flags);

ObjectProperty *object_property_add_alias(Object *obj, const char *name,
                               Object *target_obj, const char *target_name);

ObjectProperty *object_property_add_const_link(Object *obj, const char *name,
                                               Object *target);

void object_property_set_description(Object *obj, const char *name,
                                     const char *description);
void object_class_property_set_description(ObjectClass *klass, const char *name,
                                           const char *description);

int object_child_foreach(Object *obj, int (*fn)(Object *child, void *opaque),
                         void *opaque);

int object_child_foreach_recursive(Object *obj,
                                   int (*fn)(Object *child, void *opaque),
                                   void *opaque);

Object *object_property_add_new_container(Object *obj, const char *name);

char *object_property_help(const char *name, const char *type,
                           QObject *defval, const char *description);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Object, object_unref)

#endif
