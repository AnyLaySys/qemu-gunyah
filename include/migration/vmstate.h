
#ifndef QEMU_VMSTATE_H
#define QEMU_VMSTATE_H

#include "hw/vmstate-if.h"

typedef struct VMStateInfo VMStateInfo;
typedef struct VMStateField VMStateField;

struct VMStateInfo {
    const char *name;
    int coroutine_mixed_fn (*get)(QEMUFile *f, void *pv, size_t size,
                                  const VMStateField *field);
    int coroutine_mixed_fn (*put)(QEMUFile *f, void *pv, size_t size,
                                  const VMStateField *field,
                                  JSONWriter *vmdesc);
};

enum VMStateFlags {
    VMS_SINGLE           = 0x001,

    VMS_POINTER          = 0x002,

    VMS_ARRAY            = 0x004,

    VMS_STRUCT           = 0x008,

    VMS_VARRAY_INT32     = 0x010,

    VMS_BUFFER           = 0x020,

    VMS_ARRAY_OF_POINTER = 0x040,

    VMS_VARRAY_UINT16    = 0x080,

    VMS_VBUFFER          = 0x100,

    VMS_MULTIPLY         = 0x200,

    VMS_VARRAY_UINT8     = 0x400,

    VMS_VARRAY_UINT32    = 0x800,

    VMS_MUST_EXIST       = 0x1000,

    VMS_ALLOC            = 0x2000,

    VMS_MULTIPLY_ELEMENTS = 0x4000,

    VMS_VSTRUCT           = 0x8000,

    VMS_END = 0x10000
};

typedef enum {
    MIG_PRI_DEFAULT = 0,
    MIG_PRI_IOMMU,              /* Must happen before PCI devices */
    MIG_PRI_PCI_BUS,            /* Must happen before IOMMU */
    MIG_PRI_VIRTIO_MEM,         /* Must happen before IOMMU */
    MIG_PRI_GICV3_ITS,          /* Must happen before PCI devices */
    MIG_PRI_GICV3,              /* Must happen before the ITS */
    MIG_PRI_MAX,
} MigrationPriority;

struct VMStateField {
    const char *name;
    const char *err_hint;
    size_t offset;
    size_t size;
    size_t start;
    int num;
    size_t num_offset;
    size_t size_offset;
    const VMStateInfo *info;
    enum VMStateFlags flags;
    const VMStateDescription *vmsd;
    int version_id;
    int struct_version_id;
    bool (*field_exists)(void *opaque, int version_id);
};

struct VMStateDescription {
    const char *name;
    bool unmigratable;
    bool early_setup;
    int version_id;
    int minimum_version_id;
    MigrationPriority priority;
    int (*pre_load)(void *opaque);
    int (*post_load)(void *opaque, int version_id);
    int (*pre_save)(void *opaque);
    int (*post_save)(void *opaque);
    bool (*needed)(void *opaque);
    bool (*dev_unplug_pending)(void *opaque);

    const VMStateField *fields;
    const VMStateDescription * const *subsections;
};

extern const VMStateInfo vmstate_info_bool;

extern const VMStateInfo vmstate_info_int8;
extern const VMStateInfo vmstate_info_int16;
extern const VMStateInfo vmstate_info_int32;
extern const VMStateInfo vmstate_info_int64;

extern const VMStateInfo vmstate_info_uint8_equal;
extern const VMStateInfo vmstate_info_uint16_equal;
extern const VMStateInfo vmstate_info_int32_equal;
extern const VMStateInfo vmstate_info_uint32_equal;
extern const VMStateInfo vmstate_info_uint64_equal;
extern const VMStateInfo vmstate_info_int32_le;

extern const VMStateInfo vmstate_info_uint8;
extern const VMStateInfo vmstate_info_uint16;
extern const VMStateInfo vmstate_info_uint32;
extern const VMStateInfo vmstate_info_uint64;
extern const VMStateInfo vmstate_info_fd;

#define VMS_NULLPTR_MARKER (0x30U) /* '0' */
extern const VMStateInfo vmstate_info_nullptr;

extern const VMStateInfo vmstate_info_cpudouble;

extern const VMStateInfo vmstate_info_timer;
extern const VMStateInfo vmstate_info_buffer;
extern const VMStateInfo vmstate_info_unused_buffer;
extern const VMStateInfo vmstate_info_tmp;
extern const VMStateInfo vmstate_info_bitmap;
extern const VMStateInfo vmstate_info_qtailq;
extern const VMStateInfo vmstate_info_gtree;
extern const VMStateInfo vmstate_info_qlist;

#define type_check_2darray(t1,t2,n,m) ((t1(*)[n][m])0 - (t2*)0)
#define type_check_array(t1,t2,n) ((t1(*)[n])0 - (t2*)0)
#define type_check_pointer(t1,t2) ((t1**)0 - (t2*)0)
#define typeof_elt_of_field(type, field) typeof(((type *)0)->field[0])
#define type_check_varray(t1, t2, f)                                 \
    (type_check(t1, typeof_elt_of_field(t2, f))                      \
     + QEMU_BUILD_BUG_ON_ZERO(!QEMU_IS_ARRAY(((t2 *)0)->f)))

#define vmstate_offset_value(_state, _field, _type)                  \
    (offsetof(_state, _field) +                                      \
     type_check(_type, typeof_field(_state, _field)))

#define vmstate_offset_pointer(_state, _field, _type)                \
    (offsetof(_state, _field) +                                      \
     type_check_pointer(_type, typeof_field(_state, _field)))

#define vmstate_offset_array(_state, _field, _type, _num)            \
    (offsetof(_state, _field) +                                      \
     type_check_array(_type, typeof_field(_state, _field), _num))

#define vmstate_offset_2darray(_state, _field, _type, _n1, _n2)      \
    (offsetof(_state, _field) +                                      \
     type_check_2darray(_type, typeof_field(_state, _field), _n1, _n2))

#define vmstate_offset_sub_array(_state, _field, _type, _start)      \
    vmstate_offset_value(_state, _field[_start], _type)

#define vmstate_offset_buffer(_state, _field)                        \
    vmstate_offset_array(_state, _field, uint8_t,                    \
                         sizeof(typeof_field(_state, _field)))

#define vmstate_offset_varray(_state, _field, _type)                 \
    (offsetof(_state, _field) +                                      \
     type_check_varray(_type, _state, _field))


#define VMSTATE_SINGLE_TEST(_field, _state, _test, _version, _info, _type) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size         = sizeof(_type),                                   \
    .info         = &(_info),                                        \
    .flags        = VMS_SINGLE,                                      \
    .offset       = vmstate_offset_value(_state, _field, _type),     \
}

#define VMSTATE_SINGLE_FULL(_field, _state, _test, _version, _info,  \
                            _type, _err_hint) {                      \
    .name         = (stringify(_field)),                             \
    .err_hint     = (_err_hint),                                     \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size         = sizeof(_type),                                   \
    .info         = &(_info),                                        \
    .flags        = VMS_SINGLE,                                      \
    .offset       = vmstate_offset_value(_state, _field, _type),     \
}

#define VMSTATE_VALIDATE(_name, _test) { \
    .name         = (_name),                                         \
    .field_exists = (_test),                                         \
    .flags        = VMS_ARRAY | VMS_MUST_EXIST,                      \
    .num          = 0, /* 0 elements: no data, only run _test */     \
}

#define VMSTATE_POINTER(_field, _state, _version, _info, _type) {    \
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_SINGLE|VMS_POINTER,                            \
    .offset     = vmstate_offset_value(_state, _field, _type),       \
}

#define VMSTATE_POINTER_TEST(_field, _state, _test, _info, _type) {  \
    .name       = (stringify(_field)),                               \
    .info       = &(_info),                                          \
    .field_exists = (_test),                                         \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_SINGLE|VMS_POINTER,                            \
    .offset     = vmstate_offset_value(_state, _field, _type),       \
}

#define VMSTATE_ARRAY(_field, _state, _num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num        = (_num),                                            \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_ARRAY,                                         \
    .offset     = vmstate_offset_array(_state, _field, _type, _num), \
}

#define VMSTATE_2DARRAY(_field, _state, _n1, _n2, _version, _info, _type) { \
    .name       = (stringify(_field)),                                      \
    .version_id = (_version),                                               \
    .num        = (_n1) * (_n2),                                            \
    .info       = &(_info),                                                 \
    .size       = sizeof(_type),                                            \
    .flags      = VMS_ARRAY,                                                \
    .offset     = vmstate_offset_2darray(_state, _field, _type, _n1, _n2),  \
}

#define VMSTATE_VARRAY_MULTIPLY(_field, _state, _field_num, _multiply, _info, _type) { \
    .name       = (stringify(_field)),                               \
    .num_offset = vmstate_offset_value(_state, _field_num, uint32_t),\
    .num        = (_multiply),                                       \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_UINT32|VMS_MULTIPLY_ELEMENTS,           \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_SUB_ARRAY(_field, _state, _start, _num, _version, _info, _type) { \
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num        = (_num),                                            \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_ARRAY,                                         \
    .offset     = vmstate_offset_sub_array(_state, _field, _type, _start), \
}

#define VMSTATE_ARRAY_INT32_UNSAFE(_field, _state, _field_num, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .num_offset = vmstate_offset_value(_state, _field_num, int32_t), \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_INT32,                                  \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_VARRAY_INT32(_field, _state, _field_num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num_offset = vmstate_offset_value(_state, _field_num, int32_t), \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_INT32|VMS_POINTER,                      \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_VARRAY_UINT32(_field, _state, _field_num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num_offset = vmstate_offset_value(_state, _field_num, uint32_t),\
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_UINT32|VMS_POINTER,                     \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_VARRAY_UINT32_ALLOC(_field, _state, _field_num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num_offset = vmstate_offset_value(_state, _field_num, uint32_t),\
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_UINT32|VMS_POINTER|VMS_ALLOC,           \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_VARRAY_UINT16_ALLOC(_field, _state, _field_num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num_offset = vmstate_offset_value(_state, _field_num, uint16_t),\
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_UINT16 | VMS_POINTER | VMS_ALLOC,       \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_VARRAY_UINT16_UNSAFE(_field, _state, _field_num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num_offset = vmstate_offset_value(_state, _field_num, uint16_t),\
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_VARRAY_UINT16,                                 \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_VSTRUCT_TEST(_field, _state, _test, _version, _vmsd, _type, _struct_version) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .struct_version_id = (_struct_version),                          \
    .field_exists = (_test),                                         \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type),                                   \
    .flags        = VMS_VSTRUCT,                                     \
    .offset       = vmstate_offset_value(_state, _field, _type),     \
}

#define VMSTATE_STRUCT_TEST(_field, _state, _test, _version, _vmsd, _type) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type),                                   \
    .flags        = VMS_STRUCT,                                      \
    .offset       = vmstate_offset_value(_state, _field, _type),     \
}

#define VMSTATE_STRUCT_POINTER_V(_field, _state, _version, _vmsd, _type) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                        \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type *),                                 \
    .flags        = VMS_STRUCT|VMS_POINTER,                          \
    .offset       = vmstate_offset_pointer(_state, _field, _type),   \
}

#define VMSTATE_STRUCT_POINTER_TEST_V(_field, _state, _test, _version, _vmsd, _type) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                        \
    .field_exists = (_test),                                         \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type *),                                 \
    .flags        = VMS_STRUCT|VMS_POINTER,                          \
    .offset       = vmstate_offset_pointer(_state, _field, _type),   \
}

#define VMSTATE_ARRAY_OF_POINTER(_field, _state, _num, _version, _info, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .num        = (_num),                                            \
    .info       = &(_info),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_ARRAY|VMS_ARRAY_OF_POINTER,                    \
    .offset     = vmstate_offset_array(_state, _field, _type, _num), \
}

#define VMSTATE_ARRAY_OF_POINTER_TO_STRUCT(_f, _s, _n, _v, _vmsd, _type) { \
    .name       = (stringify(_f)),                                   \
    .version_id = (_v),                                              \
    .num        = (_n),                                              \
    .vmsd       = &(_vmsd),                                          \
    .size       = sizeof(_type *),                                    \
    .flags      = VMS_ARRAY|VMS_STRUCT|VMS_ARRAY_OF_POINTER,         \
    .offset     = vmstate_offset_array(_s, _f, _type*, _n),          \
}

#define VMSTATE_STRUCT_SUB_ARRAY(_field, _state, _start, _num, _version, _vmsd, _type) { \
    .name       = (stringify(_field)),                                     \
    .version_id = (_version),                                              \
    .num        = (_num),                                                  \
    .vmsd       = &(_vmsd),                                                \
    .size       = sizeof(_type),                                           \
    .flags      = VMS_STRUCT|VMS_ARRAY,                                    \
    .offset     = vmstate_offset_sub_array(_state, _field, _type, _start), \
}

#define VMSTATE_STRUCT_ARRAY_TEST(_field, _state, _num, _test, _version, _vmsd, _type) { \
    .name         = (stringify(_field)),                             \
    .num          = (_num),                                          \
    .field_exists = (_test),                                         \
    .version_id   = (_version),                                      \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type),                                   \
    .flags        = VMS_STRUCT|VMS_ARRAY,                            \
    .offset       = vmstate_offset_array(_state, _field, _type, _num),\
}

#define VMSTATE_STRUCT_2DARRAY_TEST(_field, _state, _n1, _n2, _test, \
                                    _version, _vmsd, _type) {        \
    .name         = (stringify(_field)),                             \
    .num          = (_n1) * (_n2),                                   \
    .field_exists = (_test),                                         \
    .version_id   = (_version),                                      \
    .vmsd         = &(_vmsd),                                        \
    .size         = sizeof(_type),                                   \
    .flags        = VMS_STRUCT | VMS_ARRAY,                          \
    .offset       = vmstate_offset_2darray(_state, _field, _type,    \
                                           _n1, _n2),                \
}

#define VMSTATE_STRUCT_VARRAY_UINT8(_field, _state, _field_num, _version, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .num_offset = vmstate_offset_value(_state, _field_num, uint8_t), \
    .version_id = (_version),                                        \
    .vmsd       = &(_vmsd),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_STRUCT|VMS_VARRAY_UINT8,                       \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_STRUCT_VARRAY_POINTER_KNOWN(_field, _state, _num, _version, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .num          = (_num),                                          \
    .version_id = (_version),                                        \
    .vmsd       = &(_vmsd),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_STRUCT|VMS_ARRAY|VMS_POINTER,                  \
    .offset     = offsetof(_state, _field),                          \
}

#define VMSTATE_STRUCT_VARRAY_POINTER_INT32(_field, _state, _field_num, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .version_id = 0,                                                 \
    .num_offset = vmstate_offset_value(_state, _field_num, int32_t), \
    .size       = sizeof(_type),                                     \
    .vmsd       = &(_vmsd),                                          \
    .flags      = VMS_POINTER | VMS_VARRAY_INT32 | VMS_STRUCT,       \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_STRUCT_VARRAY_POINTER_UINT32(_field, _state, _field_num, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .version_id = 0,                                                 \
    .num_offset = vmstate_offset_value(_state, _field_num, uint32_t),\
    .size       = sizeof(_type),                                     \
    .vmsd       = &(_vmsd),                                          \
    .flags      = VMS_POINTER | VMS_VARRAY_INT32 | VMS_STRUCT,       \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_STRUCT_VARRAY_POINTER_UINT16(_field, _state, _field_num, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .version_id = 0,                                                 \
    .num_offset = vmstate_offset_value(_state, _field_num, uint16_t),\
    .size       = sizeof(_type),                                     \
    .vmsd       = &(_vmsd),                                          \
    .flags      = VMS_POINTER | VMS_VARRAY_UINT16 | VMS_STRUCT,      \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_STRUCT_VARRAY_INT32(_field, _state, _field_num, _version, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .num_offset = vmstate_offset_value(_state, _field_num, int32_t), \
    .version_id = (_version),                                        \
    .vmsd       = &(_vmsd),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_STRUCT|VMS_VARRAY_INT32,                       \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_STRUCT_VARRAY_UINT32(_field, _state, _field_num, _version, _vmsd, _type) { \
    .name       = (stringify(_field)),                               \
    .num_offset = vmstate_offset_value(_state, _field_num, uint32_t), \
    .version_id = (_version),                                        \
    .vmsd       = &(_vmsd),                                          \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_STRUCT|VMS_VARRAY_UINT32,                      \
    .offset     = vmstate_offset_varray(_state, _field, _type),      \
}

#define VMSTATE_STRUCT_VARRAY_ALLOC(_field, _state, _field_num, _version, _vmsd, _type) {\
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .vmsd       = &(_vmsd),                                          \
    .num_offset = vmstate_offset_value(_state, _field_num, int32_t), \
    .size       = sizeof(_type),                                     \
    .flags      = VMS_STRUCT|VMS_VARRAY_INT32|VMS_ALLOC|VMS_POINTER, \
    .offset     = vmstate_offset_pointer(_state, _field, _type),     \
}

#define VMSTATE_STATIC_BUFFER(_field, _state, _version, _test, _start, _size) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size         = (_size - _start),                                \
    .info         = &vmstate_info_buffer,                            \
    .flags        = VMS_BUFFER,                                      \
    .offset       = vmstate_offset_buffer(_state, _field) + _start,  \
}

#define VMSTATE_VBUFFER_MULTIPLY(_field, _state, _version, _test,    \
                                 _field_size, _multiply) {           \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size_offset  = vmstate_offset_value(_state, _field_size, uint32_t),\
    .size         = (_multiply),                                      \
    .info         = &vmstate_info_buffer,                            \
    .flags        = VMS_VBUFFER|VMS_POINTER|VMS_MULTIPLY,            \
    .offset       = offsetof(_state, _field),                        \
}

#define VMSTATE_VBUFFER(_field, _state, _version, _test, _field_size) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size_offset  = vmstate_offset_value(_state, _field_size, int32_t),\
    .info         = &vmstate_info_buffer,                            \
    .flags        = VMS_VBUFFER|VMS_POINTER,                         \
    .offset       = offsetof(_state, _field),                        \
}

#define VMSTATE_VBUFFER_UINT32(_field, _state, _version, _test, _field_size) { \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size_offset  = vmstate_offset_value(_state, _field_size, uint32_t),\
    .info         = &vmstate_info_buffer,                            \
    .flags        = VMS_VBUFFER|VMS_POINTER,                         \
    .offset       = offsetof(_state, _field),                        \
}

#define VMSTATE_VBUFFER_ALLOC_UINT32(_field, _state, _version,       \
                                     _test, _field_size) {           \
    .name         = (stringify(_field)),                             \
    .version_id   = (_version),                                      \
    .field_exists = (_test),                                         \
    .size_offset  = vmstate_offset_value(_state, _field_size, uint32_t),\
    .info         = &vmstate_info_buffer,                            \
    .flags        = VMS_VBUFFER|VMS_POINTER|VMS_ALLOC,               \
    .offset       = offsetof(_state, _field),                        \
}

#define VMSTATE_BUFFER_UNSAFE_INFO_TEST(_field, _state, _test, _version, _info, _size) { \
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .field_exists = (_test),                                         \
    .size       = (_size),                                           \
    .info       = &(_info),                                          \
    .flags      = VMS_BUFFER,                                        \
    .offset     = offsetof(_state, _field),                          \
}

#define VMSTATE_BUFFER_POINTER_UNSAFE(_field, _state, _version, _size) { \
    .name       = (stringify(_field)),                               \
    .version_id = (_version),                                        \
    .size       = (_size),                                           \
    .info       = &vmstate_info_buffer,                              \
    .flags      = VMS_BUFFER|VMS_POINTER,                            \
    .offset     = offsetof(_state, _field),                          \
}

#define VMSTATE_WITH_TMP_TEST(_state, _test, _tmp_type, _vmsd) {     \
    .name         = "tmp",                                           \
    .field_exists = (_test),                                         \
    .size         = sizeof(_tmp_type) +                              \
                    QEMU_BUILD_BUG_ON_ZERO(offsetof(_tmp_type, parent) != 0) + \
                    type_check_pointer(_state,                       \
                        typeof_field(_tmp_type, parent)),            \
    .vmsd         = &(_vmsd),                                        \
    .info         = &vmstate_info_tmp,                               \
}

#define VMSTATE_WITH_TMP(_state, _tmp_type, _vmsd) \
    VMSTATE_WITH_TMP_TEST(_state, NULL, _tmp_type, _vmsd)

#define VMSTATE_UNUSED_BUFFER(_test, _version, _size) {              \
    .name         = "unused",                                        \
    .field_exists = (_test),                                         \
    .version_id   = (_version),                                      \
    .size         = (_size),                                         \
    .info         = &vmstate_info_unused_buffer,                     \
    .flags        = VMS_BUFFER,                                      \
}

#define VMSTATE_UNUSED_VARRAY_UINT32(_state, _test, _version, _field_num, _size) {\
    .name         = "unused",                                        \
    .field_exists = (_test),                                         \
    .num_offset   = vmstate_offset_value(_state, _field_num, uint32_t),\
    .version_id   = (_version),                                      \
    .size         = (_size),                                         \
    .info         = &vmstate_info_unused_buffer,                     \
    .flags        = VMS_VARRAY_UINT32 | VMS_BUFFER,                  \
}

#define VMSTATE_BITMAP_TEST(_field, _state, _test, _version, _field_size) { \
    .name         = (stringify(_field)),                             \
    .field_exists = (_test),                                         \
    .version_id   = (_version),                                      \
    .size_offset  = vmstate_offset_value(_state, _field_size, int32_t),\
    .info         = &vmstate_info_bitmap,                            \
    .flags        = VMS_VBUFFER|VMS_POINTER,                         \
    .offset       = offsetof(_state, _field),                        \
}

#define VMSTATE_BITMAP(_field, _state, _version, _field_size) \
    VMSTATE_BITMAP_TEST(_field, _state, NULL, _version, _field_size)

#define VMSTATE_QTAILQ_V(_field, _state, _version, _vmsd, _type, _next)  \
{                                                                        \
    .name         = (stringify(_field)),                                 \
    .version_id   = (_version),                                          \
    .vmsd         = &(_vmsd),                                            \
    .size         = sizeof(_type),                                       \
    .info         = &vmstate_info_qtailq,                                \
    .offset       = offsetof(_state, _field),                            \
    .start        = offsetof(_type, _next),                              \
}

#define VMSTATE_GTREE_V(_field, _state, _version, _vmsd,                       \
                        _key_type, _val_type)                                  \
{                                                                              \
    .name         = (stringify(_field)),                                       \
    .version_id   = (_version),                                                \
    .vmsd         = (_vmsd),                                                   \
    .info         = &vmstate_info_gtree,                                       \
    .start        = sizeof(_key_type),                                         \
    .size         = sizeof(_val_type),                                         \
    .offset       = offsetof(_state, _field),                                  \
}

#define VMSTATE_GTREE_DIRECT_KEY_V(_field, _state, _version, _vmsd, _val_type) \
{                                                                              \
    .name         = (stringify(_field)),                                       \
    .version_id   = (_version),                                                \
    .vmsd         = (_vmsd),                                                   \
    .info         = &vmstate_info_gtree,                                       \
    .start        = 0,                                                         \
    .size         = sizeof(_val_type),                                         \
    .offset       = offsetof(_state, _field),                                  \
}

#define VMSTATE_QLIST_V(_field, _state, _version, _vmsd, _type, _next)  \
{                                                                        \
    .name         = (stringify(_field)),                                 \
    .version_id   = (_version),                                          \
    .vmsd         = &(_vmsd),                                            \
    .size         = sizeof(_type),                                       \
    .info         = &vmstate_info_qlist,                                 \
    .offset       = offsetof(_state, _field),                            \
    .start        = offsetof(_type, _next),                              \
}


#define VMSTATE_SINGLE(_field, _state, _version, _info, _type)        \
    VMSTATE_SINGLE_TEST(_field, _state, NULL, _version, _info, _type)

#define VMSTATE_VSTRUCT(_field, _state, _vmsd, _type, _struct_version)\
    VMSTATE_VSTRUCT_TEST(_field, _state, NULL, 0, _vmsd, _type, _struct_version)

#define VMSTATE_VSTRUCT_V(_field, _state, _version, _vmsd, _type, _struct_version) \
    VMSTATE_VSTRUCT_TEST(_field, _state, NULL, _version, _vmsd, _type, \
                         _struct_version)

#define VMSTATE_STRUCT(_field, _state, _version, _vmsd, _type)        \
    VMSTATE_STRUCT_TEST(_field, _state, NULL, _version, _vmsd, _type)

#define VMSTATE_STRUCT_POINTER(_field, _state, _vmsd, _type)          \
    VMSTATE_STRUCT_POINTER_V(_field, _state, 0, _vmsd, _type)

#define VMSTATE_STRUCT_POINTER_TEST(_field, _state, _test, _vmsd, _type)     \
    VMSTATE_STRUCT_POINTER_TEST_V(_field, _state, _test, 0, _vmsd, _type)

#define VMSTATE_STRUCT_ARRAY(_field, _state, _num, _version, _vmsd, _type) \
    VMSTATE_STRUCT_ARRAY_TEST(_field, _state, _num, NULL, _version,   \
            _vmsd, _type)

#define VMSTATE_STRUCT_2DARRAY(_field, _state, _n1, _n2, _version,    \
            _vmsd, _type)                                             \
    VMSTATE_STRUCT_2DARRAY_TEST(_field, _state, _n1, _n2, NULL,       \
            _version, _vmsd, _type)

#define VMSTATE_BUFFER_UNSAFE_INFO(_field, _state, _version, _info, _size) \
    VMSTATE_BUFFER_UNSAFE_INFO_TEST(_field, _state, NULL, _version, _info, \
            _size)

#define VMSTATE_BOOL_V(_f, _s, _v)                                    \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_bool, bool)

#define VMSTATE_INT8_V(_f, _s, _v)                                    \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_int8, int8_t)
#define VMSTATE_INT16_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_int16, int16_t)
#define VMSTATE_INT32_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_int32, int32_t)
#define VMSTATE_INT64_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_int64, int64_t)

#define VMSTATE_UINT8_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint8, uint8_t)
#define VMSTATE_UINT16_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint16, uint16_t)
#define VMSTATE_UINT32_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint32, uint32_t)
#define VMSTATE_UINT64_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint64, uint64_t)

#define VMSTATE_FD_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_fd, int32_t)

#ifdef CONFIG_LINUX

#define VMSTATE_U8_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint8, __u8)
#define VMSTATE_U16_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint16, __u16)
#define VMSTATE_U32_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint32, __u32)
#define VMSTATE_U64_V(_f, _s, _v)                                  \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_uint64, __u64)

#endif

#define VMSTATE_BOOL(_f, _s)                                          \
    VMSTATE_BOOL_V(_f, _s, 0)

#define VMSTATE_INT8(_f, _s)                                          \
    VMSTATE_INT8_V(_f, _s, 0)
#define VMSTATE_INT16(_f, _s)                                         \
    VMSTATE_INT16_V(_f, _s, 0)
#define VMSTATE_INT32(_f, _s)                                         \
    VMSTATE_INT32_V(_f, _s, 0)
#define VMSTATE_INT64(_f, _s)                                         \
    VMSTATE_INT64_V(_f, _s, 0)

#define VMSTATE_UINT8(_f, _s)                                         \
    VMSTATE_UINT8_V(_f, _s, 0)
#define VMSTATE_UINT16(_f, _s)                                        \
    VMSTATE_UINT16_V(_f, _s, 0)
#define VMSTATE_UINT32(_f, _s)                                        \
    VMSTATE_UINT32_V(_f, _s, 0)
#define VMSTATE_UINT64(_f, _s)                                        \
    VMSTATE_UINT64_V(_f, _s, 0)

#define VMSTATE_FD(_f, _s)                                            \
    VMSTATE_FD_V(_f, _s, 0)

#ifdef CONFIG_LINUX

#define VMSTATE_U8(_f, _s)                                         \
    VMSTATE_U8_V(_f, _s, 0)
#define VMSTATE_U16(_f, _s)                                        \
    VMSTATE_U16_V(_f, _s, 0)
#define VMSTATE_U32(_f, _s)                                        \
    VMSTATE_U32_V(_f, _s, 0)
#define VMSTATE_U64(_f, _s)                                        \
    VMSTATE_U64_V(_f, _s, 0)

#endif

#define VMSTATE_UINT8_EQUAL(_f, _s, _err_hint)                        \
    VMSTATE_SINGLE_FULL(_f, _s, 0, 0,                                 \
                        vmstate_info_uint8_equal, uint8_t, _err_hint)

#define VMSTATE_UINT16_EQUAL(_f, _s, _err_hint)                       \
    VMSTATE_SINGLE_FULL(_f, _s, 0, 0,                                 \
                        vmstate_info_uint16_equal, uint16_t, _err_hint)

#define VMSTATE_UINT16_EQUAL_V(_f, _s, _v, _err_hint)                 \
    VMSTATE_SINGLE_FULL(_f, _s, 0,  _v,                               \
                        vmstate_info_uint16_equal, uint16_t, _err_hint)

#define VMSTATE_INT32_EQUAL(_f, _s, _err_hint)                        \
    VMSTATE_SINGLE_FULL(_f, _s, 0, 0,                                 \
                        vmstate_info_int32_equal, int32_t, _err_hint)

#define VMSTATE_UINT32_EQUAL_V(_f, _s, _v, _err_hint)                 \
    VMSTATE_SINGLE_FULL(_f, _s, 0,  _v,                               \
                        vmstate_info_uint32_equal, uint32_t, _err_hint)

#define VMSTATE_UINT32_EQUAL(_f, _s, _err_hint)                       \
    VMSTATE_UINT32_EQUAL_V(_f, _s, 0, _err_hint)

#define VMSTATE_UINT64_EQUAL_V(_f, _s, _v, _err_hint)                 \
    VMSTATE_SINGLE_FULL(_f, _s, 0,  _v,                               \
                        vmstate_info_uint64_equal, uint64_t, _err_hint)

#define VMSTATE_UINT64_EQUAL(_f, _s, _err_hint)                       \
    VMSTATE_UINT64_EQUAL_V(_f, _s, 0, _err_hint)

#define VMSTATE_INT32_POSITIVE_LE(_f, _s)                             \
    VMSTATE_SINGLE(_f, _s, 0, vmstate_info_int32_le, int32_t)

#define VMSTATE_BOOL_TEST(_f, _s, _t)                               \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_bool, bool)

#define VMSTATE_INT8_TEST(_f, _s, _t)                               \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_int8, int8_t)

#define VMSTATE_INT16_TEST(_f, _s, _t)                               \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_int16, int16_t)

#define VMSTATE_INT32_TEST(_f, _s, _t)                                  \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_int32, int32_t)

#define VMSTATE_INT64_TEST(_f, _s, _t)                                  \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_int64, int64_t)

#define VMSTATE_UINT8_TEST(_f, _s, _t)                               \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_uint8, uint8_t)

#define VMSTATE_UINT16_TEST(_f, _s, _t)                               \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_uint16, uint16_t)

#define VMSTATE_UINT32_TEST(_f, _s, _t)                                  \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_uint32, uint32_t)

#define VMSTATE_UINT64_TEST(_f, _s, _t)                                  \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_uint64, uint64_t)

#define VMSTATE_FD_TEST(_f, _s, _t)                                            \
    VMSTATE_SINGLE_TEST(_f, _s, _t, 0, vmstate_info_fd, int32_t)

#define VMSTATE_TIMER_PTR_TEST(_f, _s, _test)                             \
    VMSTATE_POINTER_TEST(_f, _s, _test, vmstate_info_timer, QEMUTimer *)

#define VMSTATE_TIMER_PTR_V(_f, _s, _v)                                   \
    VMSTATE_POINTER(_f, _s, _v, vmstate_info_timer, QEMUTimer *)

#define VMSTATE_TIMER_PTR(_f, _s)                                         \
    VMSTATE_TIMER_PTR_V(_f, _s, 0)

#define VMSTATE_TIMER_PTR_ARRAY(_f, _s, _n)                              \
    VMSTATE_ARRAY_OF_POINTER(_f, _s, _n, 0, vmstate_info_timer, QEMUTimer *)

#define VMSTATE_TIMER_TEST(_f, _s, _test)                             \
    VMSTATE_SINGLE_TEST(_f, _s, _test, 0, vmstate_info_timer, QEMUTimer)

#define VMSTATE_TIMER_V(_f, _s, _v)                                   \
    VMSTATE_SINGLE(_f, _s, _v, vmstate_info_timer, QEMUTimer)

#define VMSTATE_TIMER(_f, _s)                                         \
    VMSTATE_TIMER_V(_f, _s, 0)

#define VMSTATE_TIMER_ARRAY(_f, _s, _n)                              \
    VMSTATE_ARRAY(_f, _s, _n, 0, vmstate_info_timer, QEMUTimer)

#define VMSTATE_BOOL_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_bool, bool)

#define VMSTATE_BOOL_ARRAY(_f, _s, _n)                               \
    VMSTATE_BOOL_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_BOOL_SUB_ARRAY(_f, _s, _start, _num)                \
    VMSTATE_SUB_ARRAY(_f, _s, _start, _num, 0, vmstate_info_bool, bool)

#define VMSTATE_UINT16_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_uint16, uint16_t)

#define VMSTATE_UINT16_2DARRAY_V(_f, _s, _n1, _n2, _v)                \
    VMSTATE_2DARRAY(_f, _s, _n1, _n2, _v, vmstate_info_uint16, uint16_t)

#define VMSTATE_UINT16_ARRAY(_f, _s, _n)                               \
    VMSTATE_UINT16_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_UINT16_SUB_ARRAY(_f, _s, _start, _num)                \
    VMSTATE_SUB_ARRAY(_f, _s, _start, _num, 0, vmstate_info_uint16, uint16_t)

#define VMSTATE_UINT16_2DARRAY(_f, _s, _n1, _n2)                      \
    VMSTATE_UINT16_2DARRAY_V(_f, _s, _n1, _n2, 0)

#define VMSTATE_UINT8_2DARRAY_V(_f, _s, _n1, _n2, _v)                 \
    VMSTATE_2DARRAY(_f, _s, _n1, _n2, _v, vmstate_info_uint8, uint8_t)

#define VMSTATE_UINT8_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_uint8, uint8_t)

#define VMSTATE_UINT8_ARRAY(_f, _s, _n)                               \
    VMSTATE_UINT8_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_UINT8_SUB_ARRAY(_f, _s, _start, _num)                \
    VMSTATE_SUB_ARRAY(_f, _s, _start, _num, 0, vmstate_info_uint8, uint8_t)

#define VMSTATE_UINT8_2DARRAY(_f, _s, _n1, _n2)                       \
    VMSTATE_UINT8_2DARRAY_V(_f, _s, _n1, _n2, 0)

#define VMSTATE_UINT32_ARRAY_V(_f, _s, _n, _v)                        \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_uint32, uint32_t)

#define VMSTATE_UINT32_2DARRAY_V(_f, _s, _n1, _n2, _v)                \
    VMSTATE_2DARRAY(_f, _s, _n1, _n2, _v, vmstate_info_uint32, uint32_t)

#define VMSTATE_UINT32_ARRAY(_f, _s, _n)                              \
    VMSTATE_UINT32_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_UINT32_SUB_ARRAY(_f, _s, _start, _num)                \
    VMSTATE_SUB_ARRAY(_f, _s, _start, _num, 0, vmstate_info_uint32, uint32_t)

#define VMSTATE_UINT32_2DARRAY(_f, _s, _n1, _n2)                      \
    VMSTATE_UINT32_2DARRAY_V(_f, _s, _n1, _n2, 0)

#define VMSTATE_UINT64_ARRAY_V(_f, _s, _n, _v)                        \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_uint64, uint64_t)

#define VMSTATE_UINT64_ARRAY(_f, _s, _n)                              \
    VMSTATE_UINT64_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_UINT64_SUB_ARRAY(_f, _s, _start, _num)                \
    VMSTATE_SUB_ARRAY(_f, _s, _start, _num, 0, vmstate_info_uint64, uint64_t)

#define VMSTATE_UINT64_2DARRAY(_f, _s, _n1, _n2)                      \
    VMSTATE_UINT64_2DARRAY_V(_f, _s, _n1, _n2, 0)

#define VMSTATE_UINT64_2DARRAY_V(_f, _s, _n1, _n2, _v)                 \
    VMSTATE_2DARRAY(_f, _s, _n1, _n2, _v, vmstate_info_uint64, uint64_t)

#define VMSTATE_INT16_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_int16, int16_t)

#define VMSTATE_INT16_ARRAY(_f, _s, _n)                               \
    VMSTATE_INT16_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_INT32_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_int32, int32_t)

#define VMSTATE_INT32_ARRAY(_f, _s, _n)                               \
    VMSTATE_INT32_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_INT64_ARRAY_V(_f, _s, _n, _v)                         \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_int64, int64_t)

#define VMSTATE_INT64_ARRAY(_f, _s, _n)                               \
    VMSTATE_INT64_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_CPUDOUBLE_ARRAY_V(_f, _s, _n, _v)                     \
    VMSTATE_ARRAY(_f, _s, _n, _v, vmstate_info_cpudouble, CPU_DoubleU)

#define VMSTATE_CPUDOUBLE_ARRAY(_f, _s, _n)                           \
    VMSTATE_CPUDOUBLE_ARRAY_V(_f, _s, _n, 0)

#define VMSTATE_BUFFER_V(_f, _s, _v)                                  \
    VMSTATE_STATIC_BUFFER(_f, _s, _v, NULL, 0, sizeof(typeof_field(_s, _f)))

#define VMSTATE_BUFFER(_f, _s)                                        \
    VMSTATE_BUFFER_V(_f, _s, 0)

#define VMSTATE_PARTIAL_BUFFER(_f, _s, _size)                         \
    VMSTATE_STATIC_BUFFER(_f, _s, 0, NULL, 0, _size)

#define VMSTATE_BUFFER_START_MIDDLE_V(_f, _s, _start, _v) \
    VMSTATE_STATIC_BUFFER(_f, _s, _v, NULL, _start, sizeof(typeof_field(_s, _f)))

#define VMSTATE_BUFFER_START_MIDDLE(_f, _s, _start) \
    VMSTATE_BUFFER_START_MIDDLE_V(_f, _s, _start, 0)

#define VMSTATE_PARTIAL_VBUFFER(_f, _s, _size)                        \
    VMSTATE_VBUFFER(_f, _s, 0, NULL, _size)

#define VMSTATE_PARTIAL_VBUFFER_UINT32(_f, _s, _size)                        \
    VMSTATE_VBUFFER_UINT32(_f, _s, 0, NULL, _size)

#define VMSTATE_BUFFER_TEST(_f, _s, _test)                            \
    VMSTATE_STATIC_BUFFER(_f, _s, 0, _test, 0, sizeof(typeof_field(_s, _f)))

#define VMSTATE_BUFFER_UNSAFE(_field, _state, _version, _size)        \
    VMSTATE_BUFFER_UNSAFE_INFO(_field, _state, _version, vmstate_info_buffer, _size)

#define VMSTATE_UNUSED_V(_v, _size)                                   \
    VMSTATE_UNUSED_BUFFER(NULL, _v, _size)

#define VMSTATE_UNUSED(_size)                                         \
    VMSTATE_UNUSED_V(0, _size)

#define VMSTATE_UNUSED_TEST(_test, _size)                             \
    VMSTATE_UNUSED_BUFFER(_test, 0, _size)

#define VMSTATE_END_OF_LIST()                                         \
    {                     \
        .flags = VMS_END, \
    }

int vmstate_load_state(QEMUFile *f, const VMStateDescription *vmsd,
                       void *opaque, int version_id);
int vmstate_save_state(QEMUFile *f, const VMStateDescription *vmsd,
                       void *opaque, JSONWriter *vmdesc);
int vmstate_save_state_with_err(QEMUFile *f, const VMStateDescription *vmsd,
                       void *opaque, JSONWriter *vmdesc, Error **errp);
int vmstate_save_state_v(QEMUFile *f, const VMStateDescription *vmsd,
                         void *opaque, JSONWriter *vmdesc,
                         int version_id, Error **errp);

bool vmstate_section_needed(const VMStateDescription *vmsd, void *opaque);

#define  VMSTATE_INSTANCE_ID_ANY  -1

int vmstate_register_with_alias_id(VMStateIf *obj, uint32_t instance_id,
                                   const VMStateDescription *vmsd,
                                   void *base, int alias_id,
                                   int required_for_version,
                                   Error **errp);

static inline int vmstate_register(VMStateIf *obj, int instance_id,
                                   const VMStateDescription *vmsd,
                                   void *opaque)
{
    return vmstate_register_with_alias_id(obj, instance_id, vmsd,
                                          opaque, -1, 0, NULL);
}

int vmstate_replace_hack_for_ppc(VMStateIf *obj, int instance_id,
                                 const VMStateDescription *vmsd,
                                 void *opaque);

static inline int vmstate_register_any(VMStateIf *obj,
                                       const VMStateDescription *vmsd,
                                       void *opaque)
{
    return vmstate_register_with_alias_id(obj, VMSTATE_INSTANCE_ID_ANY, vmsd,
                                          opaque, -1, 0, NULL);
}

void vmstate_unregister(VMStateIf *obj, const VMStateDescription *vmsd,
                        void *opaque);

void vmstate_register_ram(struct MemoryRegion *memory, DeviceState *dev);
void vmstate_unregister_ram(struct MemoryRegion *memory, DeviceState *dev);
void vmstate_register_ram_global(struct MemoryRegion *memory);

bool vmstate_check_only_migratable(const VMStateDescription *vmsd);

#endif
