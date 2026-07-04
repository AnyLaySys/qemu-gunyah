#ifndef QEMU_QDEV_PROPERTIES_H
#define QEMU_QDEV_PROPERTIES_H

#include "hw/qdev-core.h"

struct Property {
    const char   *name;
    const PropertyInfo *info;
    ptrdiff_t    offset;
    const char   *link_type;
    uint64_t     bitmask;
    union {
        int64_t i;
        uint64_t u;
    } defval;
    const PropertyInfo *arrayinfo;
    int          arrayoffset;
    int          arrayfieldsize;
    uint8_t      bitnr;
    bool         set_default;
};

struct PropertyInfo {
    const char *type;
    const char *description;
    const QEnumLookup *enum_table;
    bool realized_set_allowed; /* allow setting property on realized device */
    int (*print)(Object *obj, const Property *prop, char *dest, size_t len);
    void (*set_default_value)(ObjectProperty *op, const Property *prop);
    ObjectProperty *(*create)(ObjectClass *oc, const char *name,
                              const Property *prop);
    ObjectPropertyAccessor *get;
    ObjectPropertyAccessor *set;
    ObjectPropertyRelease *release;
};



extern const PropertyInfo qdev_prop_bit;
extern const PropertyInfo qdev_prop_bit64;
extern const PropertyInfo qdev_prop_bool;
extern const PropertyInfo qdev_prop_uint8;
extern const PropertyInfo qdev_prop_uint16;
extern const PropertyInfo qdev_prop_uint32;
extern const PropertyInfo qdev_prop_usize;
extern const PropertyInfo qdev_prop_int32;
extern const PropertyInfo qdev_prop_uint64;
extern const PropertyInfo qdev_prop_uint64_checkmask;
extern const PropertyInfo qdev_prop_int64;
extern const PropertyInfo qdev_prop_size;
extern const PropertyInfo qdev_prop_string;
extern const PropertyInfo qdev_prop_on_off_auto;
extern const PropertyInfo qdev_prop_size32;
extern const PropertyInfo qdev_prop_array;
extern const PropertyInfo qdev_prop_link;

#define DEFINE_PROP(_name, _state, _field, _prop, _type, ...) {  \
        .name      = (_name),                                    \
        .info      = &(_prop),                                   \
        .offset    = offsetof(_state, _field)                    \
            + type_check(_type, typeof_field(_state, _field)),   \
        __VA_ARGS__                                              \
        }

#define DEFINE_PROP_SIGNED(_name, _state, _field, _defval, _prop, _type) \
    DEFINE_PROP(_name, _state, _field, _prop, _type,                     \
                .set_default = true,                                     \
                .defval.i    = (_type)_defval)

#define DEFINE_PROP_SIGNED_NODEFAULT(_name, _state, _field, _prop, _type) \
    DEFINE_PROP(_name, _state, _field, _prop, _type)

#define DEFINE_PROP_BIT(_name, _state, _field, _bit, _defval)   \
    DEFINE_PROP(_name, _state, _field, qdev_prop_bit, uint32_t, \
                .bitnr       = (_bit),                          \
                .set_default = true,                            \
                .defval.u    = (bool)_defval)

#define DEFINE_PROP_UNSIGNED(_name, _state, _field, _defval, _prop, _type) \
    DEFINE_PROP(_name, _state, _field, _prop, _type,                       \
                .set_default = true,                                       \
                .defval.u  = (_type)_defval)

#define DEFINE_PROP_UNSIGNED_NODEFAULT(_name, _state, _field, _prop, _type) \
    DEFINE_PROP(_name, _state, _field, _prop, _type)

#define DEFINE_PROP_BIT64(_name, _state, _field, _bit, _defval)   \
    DEFINE_PROP(_name, _state, _field, qdev_prop_bit64, uint64_t, \
                .bitnr    = (_bit),                               \
                .set_default = true,                              \
                .defval.u  = (bool)_defval)

#define DEFINE_PROP_BOOL(_name, _state, _field, _defval)     \
    DEFINE_PROP(_name, _state, _field, qdev_prop_bool, bool, \
                .set_default = true,                         \
                .defval.u    = (bool)_defval)

#define DEFINE_PROP_UINT64_CHECKMASK(_name, _state, _field, _bitmask)   \
    DEFINE_PROP(_name, _state, _field, qdev_prop_uint64_checkmask, uint64_t, \
                .bitmask    = (_bitmask),                     \
                .set_default = false)

#define DEFINE_PROP_ARRAY(_name, _state, _field,                        \
                          _arrayfield, _arrayprop, _arraytype)          \
    DEFINE_PROP(_name, _state, _field, qdev_prop_array, uint32_t,       \
                .set_default = true,                                    \
                .defval.u = 0,                                          \
                .arrayinfo = &(_arrayprop),                             \
                .arrayfieldsize = sizeof(_arraytype),                   \
                .arrayoffset = offsetof(_state, _arrayfield))

#define DEFINE_PROP_LINK(_name, _state, _field, _type, _ptr_type)     \
    DEFINE_PROP(_name, _state, _field, qdev_prop_link, _ptr_type,     \
                .link_type  = _type)

#define DEFINE_PROP_UINT8(_n, _s, _f, _d)                       \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint8, uint8_t)
#define DEFINE_PROP_UINT16(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint16, uint16_t)
#define DEFINE_PROP_UINT32(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint32, uint32_t)
#define DEFINE_PROP_INT32(_n, _s, _f, _d)                      \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_int32, int32_t)
#define DEFINE_PROP_UINT64(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint64, uint64_t)
#define DEFINE_PROP_INT64(_n, _s, _f, _d)                      \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_int64, int64_t)
#define DEFINE_PROP_SIZE(_n, _s, _f, _d)                       \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_size, uint64_t)
#define DEFINE_PROP_STRING(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_string, char*)
#define DEFINE_PROP_ON_OFF_AUTO(_n, _s, _f, _d) \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_on_off_auto, OnOffAuto)
#define DEFINE_PROP_SIZE32(_n, _s, _f, _d)                       \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_size32, uint32_t)

bool qdev_prop_set_drive_err(DeviceState *dev, const char *name,
                             BlockBackend *value, Error **errp);

void qdev_prop_set_bit(DeviceState *dev, const char *name, bool value);
void qdev_prop_set_uint8(DeviceState *dev, const char *name, uint8_t value);
void qdev_prop_set_uint16(DeviceState *dev, const char *name, uint16_t value);
void qdev_prop_set_uint32(DeviceState *dev, const char *name, uint32_t value);
void qdev_prop_set_int32(DeviceState *dev, const char *name, int32_t value);
void qdev_prop_set_uint64(DeviceState *dev, const char *name, uint64_t value);
void qdev_prop_set_string(DeviceState *dev, const char *name, const char *value);
void qdev_prop_set_chr(DeviceState *dev, const char *name, Chardev *value);
void qdev_prop_set_netdev(DeviceState *dev, const char *name, NetClientState *value);
void qdev_prop_set_drive(DeviceState *dev, const char *name,
                         BlockBackend *value);
void qdev_prop_set_macaddr(DeviceState *dev, const char *name,
                           const uint8_t *value);
void qdev_prop_set_enum(DeviceState *dev, const char *name, int value);

void qdev_prop_set_array(DeviceState *dev, const char *name, QList *values);

void *object_field_prop_ptr(Object *obj, const Property *prop);

void qdev_prop_register_global(GlobalProperty *prop);
const GlobalProperty *qdev_find_global_prop(Object *obj,
                                            const char *name);
int qdev_prop_check_globals(void);
void qdev_prop_set_globals(DeviceState *dev);
void error_set_from_qdev_prop_error(Error **errp, int ret, Object *obj,
                                    const char *name, const char *value);

void qdev_property_add_static(DeviceState *dev, const Property *prop);

void qdev_alias_all_properties(DeviceState *target, Object *source);

void qdev_prop_set_after_realize(DeviceState *dev, const char *name,
                                 Error **errp);

void qdev_prop_allow_set_link_before_realize(const Object *obj,
                                             const char *name,
                                             Object *val, Error **errp);

#endif
