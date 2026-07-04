

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/qapi-events-qdev.h"
#include "qobject/qdict.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/qdev-clock.h"
#include "migration/vmstate.h"
#include "trace.h"

static bool qdev_hot_added = false;
bool qdev_hot_removed = false;

const VMStateDescription *qdev_get_vmsd(DeviceState *dev)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);
    return dc->vmsd;
}

static void bus_free_bus_child(BusChild *kid)
{
    object_unref(OBJECT(kid->child));
    g_free(kid);
}

static void bus_remove_child(BusState *bus, DeviceState *child)
{
    BusChild *kid;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        if (kid->child == child) {
            char name[32];

            snprintf(name, sizeof(name), "child[%d]", kid->index);
            QTAILQ_REMOVE_RCU(&bus->children, kid, sibling);

            bus->num_children--;

            object_property_del(OBJECT(bus), name);

            call_rcu(kid, bus_free_bus_child, rcu);
            break;
        }
    }
}

static void bus_add_child(BusState *bus, DeviceState *child)
{
    char name[32];
    BusChild *kid = g_malloc0(sizeof(*kid));

    bus->num_children++;
    kid->index = bus->max_index++;
    kid->child = child;
    object_ref(OBJECT(kid->child));

    QTAILQ_INSERT_HEAD_RCU(&bus->children, kid, sibling);

    snprintf(name, sizeof(name), "child[%d]", kid->index);
    object_property_add_link(OBJECT(bus), name,
                             object_get_typename(OBJECT(child)),
                             (Object **)&kid->child,
                             NULL, /* read-only property */
                             0);
}

static bool bus_check_address(BusState *bus, DeviceState *child, Error **errp)
{
    BusClass *bc = BUS_GET_CLASS(bus);
    return !bc->check_address || bc->check_address(bus, child, errp);
}

bool qdev_set_parent_bus(DeviceState *dev, BusState *bus, Error **errp)
{
    BusState *old_parent_bus = dev->parent_bus;
    DeviceClass *dc = DEVICE_GET_CLASS(dev);

    assert(dc->bus_type && object_dynamic_cast(OBJECT(bus), dc->bus_type));

    if (!bus_check_address(bus, dev, errp)) {
        return false;
    }

    if (old_parent_bus) {
        trace_qdev_update_parent_bus(dev, object_get_typename(OBJECT(dev)),
            old_parent_bus, object_get_typename(OBJECT(old_parent_bus)),
            OBJECT(bus), object_get_typename(OBJECT(bus)));
        object_ref(OBJECT(dev));
        bus_remove_child(dev->parent_bus, dev);
    }
    dev->parent_bus = bus;
    object_ref(OBJECT(bus));
    bus_add_child(bus, dev);
    if (dev->realized) {
        resettable_change_parent(OBJECT(dev), OBJECT(bus),
                                 OBJECT(old_parent_bus));
    }
    if (old_parent_bus) {
        object_unref(OBJECT(old_parent_bus));
        object_unref(OBJECT(dev));
    }
    return true;
}

DeviceState *qdev_new(const char *name)
{
    return DEVICE(object_new(name));
}

DeviceState *qdev_try_new(const char *name)
{
    ObjectClass *oc = module_object_class_by_name(name);
    if (!oc) {
        return NULL;
    }
    return DEVICE(object_new_with_class(oc));
}

static QTAILQ_HEAD(, DeviceListener) device_listeners
    = QTAILQ_HEAD_INITIALIZER(device_listeners);

enum ListenerDirection { Forward, Reverse };

#define DEVICE_LISTENER_CALL(_callback, _direction, _args...)     \
    do {                                                          \
        DeviceListener *_listener;                                \
                                                                  \
        switch (_direction) {                                     \
        case Forward:                                             \
            QTAILQ_FOREACH(_listener, &device_listeners, link) {  \
                if (_listener->_callback) {                       \
                    _listener->_callback(_listener, ##_args);     \
                }                                                 \
            }                                                     \
            break;                                                \
        case Reverse:                                             \
            QTAILQ_FOREACH_REVERSE(_listener, &device_listeners,  \
                                   link) {                        \
                if (_listener->_callback) {                       \
                    _listener->_callback(_listener, ##_args);     \
                }                                                 \
            }                                                     \
            break;                                                \
        default:                                                  \
            abort();                                              \
        }                                                         \
    } while (0)

static int device_listener_add(DeviceState *dev, void *opaque)
{
    DEVICE_LISTENER_CALL(realize, Forward, dev);

    return 0;
}

void device_listener_register(DeviceListener *listener)
{
    QTAILQ_INSERT_TAIL(&device_listeners, listener, link);

    qbus_walk_children(sysbus_get_default(), NULL, NULL, device_listener_add,
                       NULL, NULL);
}

void device_listener_unregister(DeviceListener *listener)
{
    QTAILQ_REMOVE(&device_listeners, listener, link);
}

bool qdev_should_hide_device(const QDict *opts, bool from_json, Error **errp)
{
    ERRP_GUARD();
    DeviceListener *listener;

    QTAILQ_FOREACH(listener, &device_listeners, link) {
        if (listener->hide_device) {
            if (listener->hide_device(listener, opts, from_json, errp)) {
                return true;
            } else if (*errp) {
                return false;
            }
        }
    }

    return false;
}

void qdev_set_legacy_instance_id(DeviceState *dev, int alias_id,
                                 int required_for_version)
{
    assert(!dev->realized);
    dev->instance_id_alias = alias_id;
    dev->alias_required_for_version = required_for_version;
}

void device_cold_reset(DeviceState *dev)
{
    resettable_reset(OBJECT(dev), RESET_TYPE_COLD);
}

bool device_is_in_reset(DeviceState *dev)
{
    return resettable_is_in_reset(OBJECT(dev));
}

static ResettableState *device_get_reset_state(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    return &dev->reset;
}

static void device_reset_child_foreach(Object *obj, ResettableChildCallback cb,
                                       void *opaque, ResetType type)
{
    DeviceState *dev = DEVICE(obj);
    BusState *bus;

    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        cb(OBJECT(bus), opaque, type);
    }
}

bool qdev_realize(DeviceState *dev, BusState *bus, Error **errp)
{
    assert(!dev->realized && !dev->parent_bus);

    if (bus) {
        if (!qdev_set_parent_bus(dev, bus, errp)) {
            return false;
        }
    } else {
        assert(!DEVICE_GET_CLASS(dev)->bus_type);
    }

    return object_property_set_bool(OBJECT(dev), "realized", true, errp);
}

bool qdev_realize_and_unref(DeviceState *dev, BusState *bus, Error **errp)
{
    bool ret;

    ret = qdev_realize(dev, bus, errp);
    object_unref(OBJECT(dev));
    return ret;
}

void qdev_unrealize(DeviceState *dev)
{
    object_property_set_bool(OBJECT(dev), "realized", false, &error_abort);
}

static int qdev_assert_realized_properly_cb(Object *obj, void *opaque)
{
    DeviceState *dev = DEVICE(object_dynamic_cast(obj, TYPE_DEVICE));
    DeviceClass *dc;

    if (dev) {
        dc = DEVICE_GET_CLASS(dev);
        assert(dev->realized);
        assert(dev->parent_bus || !dc->bus_type);
    }
    return 0;
}

void qdev_assert_realized_properly(void)
{
    object_child_foreach_recursive(object_get_root(),
                                   qdev_assert_realized_properly_cb, NULL);
}

bool qdev_machine_modified(void)
{
    return qdev_hot_added || qdev_hot_removed;
}

BusState *qdev_get_parent_bus(const DeviceState *dev)
{
    return dev->parent_bus;
}

BusState *qdev_get_child_bus(DeviceState *dev, const char *name)
{
    BusState *bus;
    Object *child = object_resolve_path_component(OBJECT(dev), name);

    bus = (BusState *)object_dynamic_cast(child, TYPE_BUS);
    if (bus) {
        return bus;
    }

    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        if (strcmp(name, bus->name) == 0) {
            return bus;
        }
    }
    return NULL;
}

int qdev_walk_children(DeviceState *dev,
                       qdev_walkerfn *pre_devfn, qbus_walkerfn *pre_busfn,
                       qdev_walkerfn *post_devfn, qbus_walkerfn *post_busfn,
                       void *opaque)
{
    BusState *bus;
    int err;

    if (pre_devfn) {
        err = pre_devfn(dev, opaque);
        if (err) {
            return err;
        }
    }

    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        err = qbus_walk_children(bus, pre_devfn, pre_busfn,
                                 post_devfn, post_busfn, opaque);
        if (err < 0) {
            return err;
        }
    }

    if (post_devfn) {
        err = post_devfn(dev, opaque);
        if (err) {
            return err;
        }
    }

    return 0;
}

DeviceState *qdev_find_recursive(BusState *bus, const char *id)
{
    BusChild *kid;
    DeviceState *ret;
    BusState *child;

    WITH_RCU_READ_LOCK_GUARD() {
        QTAILQ_FOREACH_RCU(kid, &bus->children, sibling) {
            DeviceState *dev = kid->child;

            if (dev->id && strcmp(dev->id, id) == 0) {
                return dev;
            }

            QLIST_FOREACH(child, &dev->child_bus, sibling) {
                ret = qdev_find_recursive(child, id);
                if (ret) {
                    return ret;
                }
            }
        }
    }
    return NULL;
}

char *qdev_get_dev_path(DeviceState *dev)
{
    BusClass *bc;

    if (!dev || !dev->parent_bus) {
        return NULL;
    }

    bc = BUS_GET_CLASS(dev->parent_bus);
    if (bc->get_dev_path) {
        return bc->get_dev_path(dev);
    }

    return NULL;
}

void qdev_add_unplug_blocker(DeviceState *dev, Error *reason)
{
    dev->unplug_blockers = g_slist_prepend(dev->unplug_blockers, reason);
}

void qdev_del_unplug_blocker(DeviceState *dev, Error *reason)
{
    dev->unplug_blockers = g_slist_remove(dev->unplug_blockers, reason);
}

bool qdev_unplug_blocked(DeviceState *dev, Error **errp)
{
    if (dev->unplug_blockers) {
        error_propagate(errp, error_copy(dev->unplug_blockers->data));
        return true;
    }

    return false;
}

static bool device_get_realized(Object *obj, Error **errp)
{
    DeviceState *dev = DEVICE(obj);
    return dev->realized;
}

static bool check_only_migratable(Object *obj, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(obj);

    if (!vmstate_check_only_migratable(dc->vmsd)) {
        error_setg(errp, "Device %s is not migratable, but "
                   "--only-migratable was specified",
                   object_get_typename(obj));
        return false;
    }

    return true;
}

static void device_set_realized(Object *obj, bool value, Error **errp)
{
    DeviceState *dev = DEVICE(obj);
    DeviceClass *dc = DEVICE_GET_CLASS(dev);
    HotplugHandler *hotplug_ctrl;
    BusState *bus;
    NamedClockList *ncl;
    Error *local_err = NULL;
    bool unattached_parent = false;
    static int unattached_count;

    if (dev->hotplugged && !dc->hotpluggable) {
        error_setg(errp, "Device '%s' does not support hotplugging",
                   object_get_typename(obj));
        return;
    }

    if (value && !dev->realized) {
        if (!check_only_migratable(obj, errp)) {
            goto fail;
        }

        if (!obj->parent) {
            gchar *name = g_strdup_printf("device[%d]", unattached_count++);

            object_property_add_child(machine_get_container("unattached"),
                                      name, obj);
            unattached_parent = true;
            g_free(name);
        }

        hotplug_ctrl = qdev_get_hotplug_handler(dev);
        if (hotplug_ctrl) {
            hotplug_handler_pre_plug(hotplug_ctrl, dev, &local_err);
            if (local_err != NULL) {
                goto fail;
            }
        }

        if (dc->realize) {
            dc->realize(dev, &local_err);
            if (local_err != NULL) {
                goto fail;
            }
        }

        DEVICE_LISTENER_CALL(realize, Forward, dev);

        g_free(dev->canonical_path);
        dev->canonical_path = object_get_canonical_path(OBJECT(dev));
        QLIST_FOREACH(ncl, &dev->clocks, node) {
            if (ncl->alias) {
                continue;
            } else {
                clock_setup_canonical_path(ncl->clock);
            }
        }

        if (qdev_get_vmsd(dev)) {
            if (vmstate_register_with_alias_id(VMSTATE_IF(dev),
                                               VMSTATE_INSTANCE_ID_ANY,
                                               qdev_get_vmsd(dev), dev,
                                               dev->instance_id_alias,
                                               dev->alias_required_for_version,
                                               &local_err) < 0) {
                goto post_realize_fail;
            }
        }

        resettable_state_clear(&dev->reset);

        QLIST_FOREACH(bus, &dev->child_bus, sibling) {
            if (!qbus_realize(bus, errp)) {
                goto child_realize_fail;
            }
        }
        if (dev->hotplugged) {
            resettable_assert_reset(OBJECT(dev), RESET_TYPE_COLD);
            resettable_change_parent(OBJECT(dev), OBJECT(dev->parent_bus),
                                     NULL);
            resettable_release_reset(OBJECT(dev), RESET_TYPE_COLD);
        }
        dev->pending_deleted_event = false;

        if (hotplug_ctrl) {
            hotplug_handler_plug(hotplug_ctrl, dev, &local_err);
            if (local_err != NULL) {
                goto child_realize_fail;
            }
       }

       qatomic_store_release(&dev->realized, value);

    } else if (!value && dev->realized) {


        qatomic_set(&dev->realized, value);
        smp_wmb();

        QLIST_FOREACH(bus, &dev->child_bus, sibling) {
            qbus_unrealize(bus);
        }
        if (qdev_get_vmsd(dev)) {
            vmstate_unregister(VMSTATE_IF(dev), qdev_get_vmsd(dev), dev);
        }
        if (dc->unrealize) {
            dc->unrealize(dev);
        }
        dev->pending_deleted_event = true;
        DEVICE_LISTENER_CALL(unrealize, Reverse, dev);
    }

    assert(local_err == NULL);
    return;

child_realize_fail:
    QLIST_FOREACH(bus, &dev->child_bus, sibling) {
        qbus_unrealize(bus);
    }

    if (qdev_get_vmsd(dev)) {
        vmstate_unregister(VMSTATE_IF(dev), qdev_get_vmsd(dev), dev);
    }

post_realize_fail:
    g_free(dev->canonical_path);
    dev->canonical_path = NULL;
    if (dc->unrealize) {
        dc->unrealize(dev);
    }

fail:
    error_propagate(errp, local_err);
    if (unattached_parent) {
        object_unparent(OBJECT(dev));
        unattached_count--;
    }
}

static bool device_get_hotpluggable(Object *obj, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(obj);
    DeviceState *dev = DEVICE(obj);

    return dc->hotpluggable && (dev->parent_bus == NULL ||
                                qbus_is_hotpluggable(dev->parent_bus));
}

static bool device_get_hotplugged(Object *obj, Error **errp)
{
    DeviceState *dev = DEVICE(obj);

    return dev->hotplugged;
}

static void device_initfn(Object *obj)
{
    DeviceState *dev = DEVICE(obj);

    if (phase_check(PHASE_MACHINE_READY)) {
        dev->hotplugged = 1;
        qdev_hot_added = true;
    }

    dev->instance_id_alias = -1;
    dev->realized = false;
    dev->allow_unplug_during_migration = false;

    QLIST_INIT(&dev->gpios);
    QLIST_INIT(&dev->clocks);
}

static void device_post_init(Object *obj)
{
    object_apply_compat_props(obj);
    qdev_prop_set_globals(DEVICE(obj));
}

static void device_finalize(Object *obj)
{
    NamedGPIOList *ngl, *next;

    DeviceState *dev = DEVICE(obj);

    g_assert(!dev->unplug_blockers);

    QLIST_FOREACH_SAFE(ngl, &dev->gpios, node, next) {
        QLIST_REMOVE(ngl, node);
        qemu_free_irqs(ngl->in, ngl->num_in);
        g_free(ngl->name);
        g_free(ngl);
    }

    qdev_finalize_clocklist(dev);

    if (dev->pending_deleted_event) {
        g_assert(dev->canonical_path);

        qapi_event_send_device_deleted(dev->id, dev->canonical_path);
        g_free(dev->canonical_path);
        dev->canonical_path = NULL;
    }

    g_free(dev->id);
}

static void device_class_base_init(ObjectClass *class, void *data)
{
    DeviceClass *klass = DEVICE_CLASS(class);

    klass->props_ = NULL;
    klass->props_count_ = 0;
}

static void device_unparent(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    BusState *bus;

    if (dev->realized) {
        qdev_unrealize(dev);
    }
    while (dev->num_child_bus) {
        bus = QLIST_FIRST(&dev->child_bus);
        object_unparent(OBJECT(bus));
    }
    if (dev->parent_bus) {
        bus_remove_child(dev->parent_bus, dev);
        object_unref(OBJECT(dev->parent_bus));
        dev->parent_bus = NULL;
    }
}

static char *
device_vmstate_if_get_id(VMStateIf *obj)
{
    DeviceState *dev = DEVICE(obj);

    return qdev_get_dev_path(dev);
}

static void device_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    VMStateIfClass *vc = VMSTATE_IF_CLASS(class);
    ResettableClass *rc = RESETTABLE_CLASS(class);

    class->unparent = device_unparent;

    dc->hotpluggable = true;
    dc->user_creatable = true;
    vc->get_id = device_vmstate_if_get_id;
    rc->get_state = device_get_reset_state;
    rc->child_foreach = device_reset_child_foreach;

    dc->legacy_reset = NULL;

    object_class_property_add_bool(class, "realized",
                                   device_get_realized, device_set_realized);
    object_class_property_add_bool(class, "hotpluggable",
                                   device_get_hotpluggable, NULL);
    object_class_property_add_bool(class, "hotplugged",
                                   device_get_hotplugged, NULL);
    object_class_property_add_link(class, "parent_bus", TYPE_BUS,
                                   offsetof(DeviceState, parent_bus), NULL, 0);
}

static void do_legacy_reset(Object *obj, ResetType type)
{
    DeviceClass *dc = DEVICE_GET_CLASS(obj);

    dc->legacy_reset(DEVICE(obj));
}

void device_class_set_legacy_reset(DeviceClass *dc, DeviceReset dev_reset)
{
    ResettableClass *rc = RESETTABLE_CLASS(dc);

    rc->phases.enter = NULL;
    rc->phases.hold = do_legacy_reset;
    rc->phases.exit = NULL;
    dc->legacy_reset = dev_reset;
}

void device_class_set_parent_realize(DeviceClass *dc,
                                     DeviceRealize dev_realize,
                                     DeviceRealize *parent_realize)
{
    *parent_realize = dc->realize;
    dc->realize = dev_realize;
}

void device_class_set_parent_unrealize(DeviceClass *dc,
                                       DeviceUnrealize dev_unrealize,
                                       DeviceUnrealize *parent_unrealize)
{
    *parent_unrealize = dc->unrealize;
    dc->unrealize = dev_unrealize;
}

Object *qdev_get_machine(void)
{
    static Object *dev;

    if (dev == NULL) {
        dev = object_resolve_path_component(object_get_root(), "machine");
        assert(dev);
    }

    return dev;
}

Object *machine_get_container(const char *name)
{
    Object *container, *machine;

    machine = qdev_get_machine();
    container = object_resolve_path_component(machine, name);
    assert(object_dynamic_cast(container, TYPE_CONTAINER));

    return container;
}

char *qdev_get_human_name(DeviceState *dev)
{
    g_assert(dev != NULL);

    return dev->id ?
           g_strdup(dev->id) : object_get_canonical_path(OBJECT(dev));
}

static MachineInitPhase machine_phase;

bool phase_check(MachineInitPhase phase)
{
    return machine_phase >= phase;
}

void phase_advance(MachineInitPhase phase)
{
    assert(machine_phase == phase - 1);
    machine_phase = phase;
}

static const TypeInfo device_type_info = {
    .name = TYPE_DEVICE,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(DeviceState),
    .instance_init = device_initfn,
    .instance_post_init = device_post_init,
    .instance_finalize = device_finalize,
    .class_base_init = device_class_base_init,
    .class_init = device_class_init,
    .abstract = true,
    .class_size = sizeof(DeviceClass),
    .interfaces = (InterfaceInfo[]) {
        { TYPE_VMSTATE_IF },
        { TYPE_RESETTABLE_INTERFACE },
        { }
    }
};

static void qdev_register_types(void)
{
    type_register_static(&device_type_info);
}

type_init(qdev_register_types)
