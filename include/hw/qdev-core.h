#ifndef QDEV_CORE_H
#define QDEV_CORE_H

#include "qemu/atomic.h"
#include "qemu/queue.h"
#include "qemu/bitmap.h"
#include "qemu/rcu.h"
#include "qemu/rcu_queue.h"
#include "qom/object.h"
#include "hw/hotplug.h"
#include "hw/resettable.h"


enum {
    DEV_NVECTORS_UNSPECIFIED = -1,
};

#define TYPE_DEVICE "device"
OBJECT_DECLARE_TYPE(DeviceState, DeviceClass, DEVICE)

typedef enum DeviceCategory {
    DEVICE_CATEGORY_BRIDGE,
    DEVICE_CATEGORY_USB,
    DEVICE_CATEGORY_STORAGE,
    DEVICE_CATEGORY_NETWORK,
    DEVICE_CATEGORY_INPUT,
    DEVICE_CATEGORY_DISPLAY,
    DEVICE_CATEGORY_SOUND,
    DEVICE_CATEGORY_MISC,
    DEVICE_CATEGORY_CPU,
    DEVICE_CATEGORY_MAX
} DeviceCategory;

typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev);
typedef void (*DeviceReset)(DeviceState *dev);
typedef void (*BusRealize)(BusState *bus, Error **errp);
typedef void (*BusUnrealize)(BusState *bus);
typedef int (*DeviceSyncConfig)(DeviceState *dev, Error **errp);

struct DeviceClass {
    ObjectClass parent_class;


    DECLARE_BITMAP(categories, DEVICE_CATEGORY_MAX);
    const char *fw_name;
    const char *desc;

    const Property *props_;

    uint16_t props_count_;

    bool user_creatable;
    bool hotpluggable;

    DeviceReset legacy_reset;
    DeviceRealize realize;
    DeviceUnrealize unrealize;
    DeviceSyncConfig sync_config;

    const VMStateDescription *vmsd;

    const char *bus_type;
};

typedef struct NamedGPIOList NamedGPIOList;

struct NamedGPIOList {
    char *name;
    qemu_irq *in;
    int num_in;
    int num_out;
    QLIST_ENTRY(NamedGPIOList) node;
};

typedef struct Clock Clock;
typedef struct NamedClockList NamedClockList;

struct NamedClockList {
    char *name;
    Clock *clock;
    bool output;
    bool alias;
    QLIST_ENTRY(NamedClockList) node;
};

typedef struct {
    bool engaged_in_io;
} MemReentrancyGuard;


typedef QLIST_HEAD(, NamedGPIOList) NamedGPIOListHead;
typedef QLIST_HEAD(, NamedClockList) NamedClockListHead;
typedef QLIST_HEAD(, BusState) BusStateHead;

struct DeviceState {
    Object parent_obj;

    char *id;
    char *canonical_path;
    bool realized;
    bool pending_deleted_event;
    int64_t pending_deleted_expires_ms;
    int hotplugged;
    bool allow_unplug_during_migration;
    BusState *parent_bus;
    NamedGPIOListHead gpios;
    NamedClockListHead clocks;
    BusStateHead child_bus;
    int num_child_bus;
    int instance_id_alias;
    int alias_required_for_version;
    ResettableState reset;
    GSList *unplug_blockers;
    MemReentrancyGuard mem_reentrancy_guard;
};

typedef struct DeviceListener DeviceListener;
struct DeviceListener {
    void (*realize)(DeviceListener *listener, DeviceState *dev);
    void (*unrealize)(DeviceListener *listener, DeviceState *dev);
    bool (*hide_device)(DeviceListener *listener, const QDict *device_opts,
                        bool from_json, Error **errp);
    QTAILQ_ENTRY(DeviceListener) link;
};

#define TYPE_BUS "bus"
DECLARE_OBJ_CHECKERS(BusState, BusClass,
                     BUS, TYPE_BUS)

struct BusClass {
    ObjectClass parent_class;

    void (*print_dev)(Monitor *mon, DeviceState *dev, int indent);
    char *(*get_dev_path)(DeviceState *dev);

    char *(*get_fw_dev_path)(DeviceState *dev);

    bool (*check_address)(BusState *bus, DeviceState *dev, Error **errp);

    BusRealize realize;
    BusUnrealize unrealize;

    int max_dev;
    int automatic_ids;
};

typedef struct BusChild {
    struct rcu_head rcu;
    DeviceState *child;
    int index;
    QTAILQ_ENTRY(BusChild) sibling;
} BusChild;

#define QDEV_HOTPLUG_HANDLER_PROPERTY "hotplug-handler"

typedef QTAILQ_HEAD(, BusChild) BusChildHead;
typedef QLIST_ENTRY(BusState) BusStateEntry;

struct BusState {
    Object obj;
    DeviceState *parent;
    char *name;
    HotplugHandler *hotplug_handler;
    int max_index;
    bool realized;
    bool full;
    int num_children;

    BusChildHead children;
    BusStateEntry sibling;
    ResettableState reset;
};

typedef struct GlobalProperty {
    const char *driver;
    const char *property;
    const char *value;
    bool used;
    bool optional;
} GlobalProperty;

DeviceState *qdev_new(const char *name);

DeviceState *qdev_try_new(const char *name);

static inline bool qdev_is_realized(DeviceState *dev)
{
    return qatomic_load_acquire(&dev->realized);
}

bool qdev_realize(DeviceState *dev, BusState *bus, Error **errp);

bool qdev_realize_and_unref(DeviceState *dev, BusState *bus, Error **errp);

void qdev_unrealize(DeviceState *dev);
void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version);
HotplugHandler *qdev_get_bus_hotplug_handler(DeviceState *dev);
HotplugHandler *qdev_get_machine_hotplug_handler(DeviceState *dev);
bool qdev_hotplug_allowed(DeviceState *dev, BusState *bus, Error **errp);
bool qdev_hotunplug_allowed(DeviceState *dev, Error **errp);

HotplugHandler *qdev_get_hotplug_handler(DeviceState *dev);
void qdev_unplug(DeviceState *dev, Error **errp);
int qdev_sync_config(DeviceState *dev, Error **errp);
void qdev_simple_device_unplug_cb(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp);
void qdev_machine_creation_done(void);
bool qdev_machine_modified(void);

void qdev_add_unplug_blocker(DeviceState *dev, Error *reason);

void qdev_del_unplug_blocker(DeviceState *dev, Error *reason);

bool qdev_unplug_blocked(DeviceState *dev, Error **errp);

typedef enum {
    GPIO_POLARITY_ACTIVE_LOW,
    GPIO_POLARITY_ACTIVE_HIGH
} GpioPolarity;

qemu_irq qdev_get_gpio_in(DeviceState *dev, int n);

qemu_irq qdev_get_gpio_in_named(DeviceState *dev, const char *name, int n);

void qdev_connect_gpio_out(DeviceState *dev, int n, qemu_irq pin);

void qdev_connect_gpio_out_named(DeviceState *dev, const char *name, int n,
                                 qemu_irq input_pin);

qemu_irq qdev_get_gpio_out_connector(DeviceState *dev, const char *name, int n);

qemu_irq qdev_intercept_gpio_out(DeviceState *dev, qemu_irq icpt,
                                 const char *name, int n);

BusState *qdev_get_child_bus(DeviceState *dev, const char *name);


void qdev_init_gpio_in(DeviceState *dev, qemu_irq_handler handler, int n);

void qdev_init_gpio_out(DeviceState *dev, qemu_irq *pins, int n);

void qdev_init_gpio_out_named(DeviceState *dev, qemu_irq *pins,
                              const char *name, int n);

void qdev_init_gpio_in_named_with_opaque(DeviceState *dev,
                                         qemu_irq_handler handler,
                                         void *opaque,
                                         const char *name, int n);

static inline void qdev_init_gpio_in_named(DeviceState *dev,
                                           qemu_irq_handler handler,
                                           const char *name, int n)
{
    qdev_init_gpio_in_named_with_opaque(dev, handler, dev, name, n);
}

void qdev_pass_gpios(DeviceState *dev, DeviceState *container,
                     const char *name);

BusState *qdev_get_parent_bus(const DeviceState *dev);


DeviceState *qdev_find_recursive(BusState *bus, const char *id);

typedef int (qbus_walkerfn)(BusState *bus, void *opaque);
typedef int (qdev_walkerfn)(DeviceState *dev, void *opaque);

void qbus_init(void *bus, size_t size, const char *typename,
               DeviceState *parent, const char *name);
BusState *qbus_new(const char *typename, DeviceState *parent, const char *name);
bool qbus_realize(BusState *bus, Error **errp);
void qbus_unrealize(BusState *bus);

int qbus_walk_children(BusState *bus,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);
int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque);

void device_cold_reset(DeviceState *dev);

void bus_cold_reset(BusState *bus);

bool device_is_in_reset(DeviceState *dev);

bool bus_is_in_reset(BusState *bus);

BusState *sysbus_get_default(void);

char *qdev_get_fw_dev_path(DeviceState *dev);
char *qdev_get_own_fw_dev_path_from_handler(BusState *bus, DeviceState *dev);

#define device_class_set_props(dc, props) \
    do {                                                                \
        QEMU_BUILD_BUG_ON(sizeof(props) == 0);                          \
        size_t props_count_ = ARRAY_SIZE(props);                        \
        if ((props)[props_count_ - 1].name == NULL) {                   \
            qemu_build_not_reached();                                   \
        }                                                               \
        device_class_set_props_n((dc), (props), props_count_);          \
    } while (0)

void device_class_set_props_n(DeviceClass *dc, const Property *props, size_t n);

void device_class_set_parent_realize(DeviceClass *dc,
                                     DeviceRealize dev_realize,
                                     DeviceRealize *parent_realize);

void device_class_set_legacy_reset(DeviceClass *dc,
                                   DeviceReset dev_reset);

void device_class_set_parent_unrealize(DeviceClass *dc,
                                       DeviceUnrealize dev_unrealize,
                                       DeviceUnrealize *parent_unrealize);

const VMStateDescription *qdev_get_vmsd(DeviceState *dev);

const char *qdev_fw_name(DeviceState *dev);

void qdev_assert_realized_properly(void);
Object *qdev_get_machine(void);

void qdev_create_fake_machine(void);

Object *machine_get_container(const char *name);

char *qdev_get_human_name(DeviceState *dev);

bool qdev_set_parent_bus(DeviceState *dev, BusState *bus, Error **errp);

extern bool qdev_hot_removed;

char *qdev_get_dev_path(DeviceState *dev);

void qbus_set_hotplug_handler(BusState *bus, Object *handler);
void qbus_set_bus_hotplug_handler(BusState *bus);

static inline bool qbus_is_hotpluggable(BusState *bus)
{
    HotplugHandler *plug_handler = bus->hotplug_handler;
    bool ret = !!plug_handler;

    if (plug_handler) {
        HotplugHandlerClass *hdc;

        hdc = HOTPLUG_HANDLER_GET_CLASS(plug_handler);
        if (hdc->is_hotpluggable_bus) {
            ret = hdc->is_hotpluggable_bus(plug_handler, bus);
        }
    }
    return ret;
}

static inline void qbus_mark_full(BusState *bus)
{
    bus->full = true;
}

void device_listener_register(DeviceListener *listener);
void device_listener_unregister(DeviceListener *listener);

bool qdev_should_hide_device(const QDict *opts, bool from_json, Error **errp);

typedef enum MachineInitPhase {
    PHASE_NO_MACHINE,

    PHASE_MACHINE_CREATED,

    PHASE_ACCEL_CREATED,

    PHASE_LATE_BACKENDS_CREATED,

    PHASE_MACHINE_INITIALIZED,

    PHASE_MACHINE_READY,
} MachineInitPhase;

bool phase_check(MachineInitPhase phase);
void phase_advance(MachineInitPhase phase);

#endif
