
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "monitor/monitor.h"
#include "monitor/qdev.h"
#include "system/arch_init.h"
#include "system/runstate.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-qdev.h"
#include "qobject/qdict.h"
#include "qapi/qmp/qerror.h"
#include "qobject/qstring.h"
#include "qapi/qobject-input-visitor.h"
#include "qemu/config-file.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/option_int.h"
#include "system/block-backend.h"
#include "qemu/cutils.h"
#include "hw/qdev-properties.h"
#include "hw/clock.h"
#include "hw/boards.h"

typedef struct QDevAlias
{
    const char *typename;
    const char *alias;
    uint32_t arch_mask;
} QDevAlias;

#define QEMU_ARCH_VIRTIO_PCI (QEMU_ARCH_ALPHA | \
                              QEMU_ARCH_ARM | \
                              QEMU_ARCH_HPPA | \
                              QEMU_ARCH_I386 | \
                              QEMU_ARCH_LOONGARCH | \
                              QEMU_ARCH_MIPS | \
                              QEMU_ARCH_OPENRISC | \
                              QEMU_ARCH_PPC | \
                              QEMU_ARCH_RISCV | \
                              QEMU_ARCH_SH4 | \
                              QEMU_ARCH_SPARC | \
                              QEMU_ARCH_XTENSA)
#define QEMU_ARCH_VIRTIO_CCW (QEMU_ARCH_S390X)
#define QEMU_ARCH_VIRTIO_MMIO (QEMU_ARCH_M68K)

static const QDevAlias qdev_alias_table[] = {
    { "virtio-blk-pci", "virtio-blk", QEMU_ARCH_VIRTIO_PCI },
    { "virtio-keyboard-pci", "virtio-keyboard", QEMU_ARCH_VIRTIO_PCI },
    { "virtio-net-pci", "virtio-net", QEMU_ARCH_VIRTIO_PCI },
    { "virtio-tablet-pci", "virtio-tablet", QEMU_ARCH_VIRTIO_PCI },
    { }
};

static const char *qdev_class_get_alias(DeviceClass *dc)
{
    const char *typename = object_class_get_name(OBJECT_CLASS(dc));
    int i;

    for (i = 0; qdev_alias_table[i].typename; i++) {
        if (qdev_alias_table[i].arch_mask &&
            !qemu_arch_available(qdev_alias_table[i].arch_mask)) {
            continue;
        }

        if (strcmp(qdev_alias_table[i].typename, typename) == 0) {
            return qdev_alias_table[i].alias;
        }
    }

    return NULL;
}

static const char *find_typename_by_alias(const char *alias)
{
    int i;

    for (i = 0; qdev_alias_table[i].alias; i++) {
        if (qdev_alias_table[i].arch_mask &&
            !qemu_arch_available(qdev_alias_table[i].arch_mask)) {
            continue;
        }

        if (strcmp(qdev_alias_table[i].alias, alias) == 0) {
            return qdev_alias_table[i].typename;
        }
    }

    return NULL;
}

static DeviceClass *qdev_get_device_class(const char **driver, Error **errp)
{
    ObjectClass *oc;
    DeviceClass *dc;
    const char *original_name = *driver;

    oc = module_object_class_by_name(*driver);
    if (!oc) {
        const char *typename = find_typename_by_alias(*driver);

        if (typename) {
            *driver = typename;
            oc = module_object_class_by_name(*driver);
        }
    }

    if (!object_class_dynamic_cast(oc, TYPE_DEVICE)) {
        if (*driver != original_name) {
            error_setg(errp, "'%s' (alias '%s') is not a valid device model"
                       " name", original_name, *driver);
        } else {
            error_setg(errp, "'%s' is not a valid device model name", *driver);
        }
        return NULL;
    }

    if (object_class_is_abstract(oc)) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "driver",
                   "a non-abstract device type");
        return NULL;
    }

    dc = DEVICE_CLASS(oc);
    if (!dc->user_creatable) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "driver",
                   "a pluggable device type");
        return NULL;
    }

    if (object_class_dynamic_cast(oc, TYPE_SYS_BUS_DEVICE)) {
        MachineClass *mc = MACHINE_CLASS(object_get_class(qdev_get_machine()));
        if (!device_type_is_dynamic_sysbus(mc, *driver)) {
            error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "driver",
                       "a dynamic sysbus device type for the machine");
            return NULL;
        }
    }

    return dc;
}


static Object *qdev_get_peripheral(void)
{
    static Object *dev;

    if (dev == NULL) {
        dev = machine_get_container("peripheral");
    }

    return dev;
}

static Object *qdev_get_peripheral_anon(void)
{
    static Object *dev;

    if (dev == NULL) {
        dev = machine_get_container("peripheral-anon");
    }

    return dev;
}

static void qbus_error_append_bus_list_hint(DeviceState *dev,
                                            Error *const *errp)
{
    BusState *child;
    const char *sep = " ";

    error_append_hint(errp, "child buses at \"%s\":",
                      dev->id ? dev->id : object_get_typename(OBJECT(dev)));
    QLIST_FOREACH(child, &dev->child_bus, sibling) {
        error_append_hint(errp, "%s\"%s\"", sep, child->name);
        sep = ", ";
    }
    error_append_hint(errp, "\n");
}

static void qbus_error_append_dev_list_hint(BusState *bus,
                                            Error *const *errp)
{
    BusChild *kid;
    const char *sep = " ";

    error_append_hint(errp, "devices at \"%s\":", bus->name);
    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;
        error_append_hint(errp, "%s\"%s\"", sep,
                          object_get_typename(OBJECT(dev)));
        if (dev->id) {
            error_append_hint(errp, "/\"%s\"", dev->id);
        }
        sep = ", ";
    }
    error_append_hint(errp, "\n");
}

static BusState *qbus_find_bus(DeviceState *dev, char *elem)
{
    BusState *child;

    QLIST_FOREACH(child, &dev->child_bus, sibling) {
        if (strcmp(child->name, elem) == 0) {
            return child;
        }
    }
    return NULL;
}

static DeviceState *qbus_find_dev(BusState *bus, char *elem)
{
    BusChild *kid;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;
        if (dev->id  &&  strcmp(dev->id, elem) == 0) {
            return dev;
        }
    }
    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;
        if (strcmp(object_get_typename(OBJECT(dev)), elem) == 0) {
            return dev;
        }
    }
    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;
        DeviceClass *dc = DEVICE_GET_CLASS(dev);

        const char *alias = qdev_class_get_alias(dc);

        if (alias && strcmp(alias, elem) == 0) {
            return dev;
        }
    }
    return NULL;
}

static inline bool qbus_is_full(BusState *bus)
{
    BusClass *bus_class;

    if (bus->full) {
        return true;
    }
    bus_class = BUS_GET_CLASS(bus);
    return bus_class->max_dev && bus->num_children >= bus_class->max_dev;
}

static BusState *qbus_find_recursive(BusState *bus, const char *name,
                                     const char *bus_typename)
{
    BusChild *kid;
    BusState *pick, *child, *ret;
    bool match;

    assert(name || bus_typename);
    if (name) {
        match = !strcmp(bus->name, name);
    } else {
        match = !!object_dynamic_cast(OBJECT(bus), bus_typename);
    }

    if (match && !qbus_is_full(bus)) {
        return bus;             /* root matches and isn't full */
    }

    pick = match ? bus : NULL;

    QTAILQ_FOREACH(kid, &bus->children, sibling) {
        DeviceState *dev = kid->child;
        QLIST_FOREACH(child, &dev->child_bus, sibling) {
            ret = qbus_find_recursive(child, name, bus_typename);
            if (ret && !qbus_is_full(ret)) {
                return ret;     /* a descendant matches and isn't full */
            }
            if (ret && !pick) {
                pick = ret;
            }
        }
    }

    return pick;
}

static BusState *qbus_find(const char *path, Error **errp)
{
    DeviceState *dev;
    BusState *bus;
    char elem[128];
    int pos, len;

    if (path[0] == '/') {
        bus = sysbus_get_default();
        pos = 0;
    } else {
        if (sscanf(path, "%127[^/]%n", elem, &len) != 1) {
            assert(!path[0]);
            elem[0] = len = 0;
        }
        bus = qbus_find_recursive(sysbus_get_default(), elem, NULL);
        if (!bus) {
            error_setg(errp, "Bus '%s' not found", elem);
            return NULL;
        }
        pos = len;
    }

    for (;;) {
        assert(path[pos] == '/' || !path[pos]);
        while (path[pos] == '/') {
            pos++;
        }
        if (path[pos] == '\0') {
            break;
        }

        if (sscanf(path+pos, "%127[^/]%n", elem, &len) != 1) {
            g_assert_not_reached();
            elem[0] = len = 0;
        }
        pos += len;
        dev = qbus_find_dev(bus, elem);
        if (!dev) {
            error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                      "Device '%s' not found", elem);
            qbus_error_append_dev_list_hint(bus, errp);
            return NULL;
        }

        assert(path[pos] == '/' || !path[pos]);
        while (path[pos] == '/') {
            pos++;
        }
        if (path[pos] == '\0') {
            if (dev->num_child_bus == 1) {
                bus = QLIST_FIRST(&dev->child_bus);
                break;
            }
            if (dev->num_child_bus) {
                error_setg(errp, "Device '%s' has multiple child buses",
                           elem);
                qbus_error_append_bus_list_hint(dev, errp);
            } else {
                error_setg(errp, "Device '%s' has no child bus", elem);
            }
            return NULL;
        }

        if (sscanf(path+pos, "%127[^/]%n", elem, &len) != 1) {
            g_assert_not_reached();
            elem[0] = len = 0;
        }
        pos += len;
        bus = qbus_find_bus(dev, elem);
        if (!bus) {
            error_setg(errp, "Bus '%s' not found", elem);
            qbus_error_append_bus_list_hint(dev, errp);
            return NULL;
        }
    }

    if (qbus_is_full(bus)) {
        error_setg(errp, "Bus '%s' is full", path);
        return NULL;
    }
    return bus;
}

const char *qdev_set_id(DeviceState *dev, char *id, Error **errp)
{
    ObjectProperty *prop;

    assert(!dev->id && !dev->realized);

    if (id) {
        prop = object_property_try_add_child(qdev_get_peripheral(), id,
                                             OBJECT(dev), NULL);
        if (prop) {
            dev->id = id;
        } else {
            error_setg(errp, "Duplicate device ID '%s'", id);
            g_free(id);
            return NULL;
        }
    } else {
        static int anon_count;
        gchar *name = g_strdup_printf("device[%d]", anon_count++);
        prop = object_property_add_child(qdev_get_peripheral_anon(), name,
                                         OBJECT(dev));
        g_free(name);
    }

    return prop->name;
}

DeviceState *qdev_device_add_from_qdict(const QDict *opts,
                                        bool from_json, Error **errp)
{
    ERRP_GUARD();
    DeviceClass *dc;
    const char *driver, *path;
    char *id;
    DeviceState *dev = NULL;
    BusState *bus = NULL;
    QDict *properties;

    driver = qdict_get_try_str(opts, "driver");
    if (!driver) {
        error_setg(errp, QERR_MISSING_PARAMETER, "driver");
        return NULL;
    }

    dc = qdev_get_device_class(&driver, errp);
    if (!dc) {
        return NULL;
    }

    path = qdict_get_try_str(opts, "bus");
    if (path != NULL) {
        bus = qbus_find(path, errp);
        if (!bus) {
            return NULL;
        }
        if (!object_dynamic_cast(OBJECT(bus), dc->bus_type)) {
            error_setg(errp, "Device '%s' can't go on %s bus",
                       driver, object_get_typename(OBJECT(bus)));
            return NULL;
        }
    } else if (dc->bus_type != NULL) {
        bus = qbus_find_recursive(sysbus_get_default(), NULL, dc->bus_type);
        if (!bus || qbus_is_full(bus)) {
            error_setg(errp, "No '%s' bus found for device '%s'",
                       dc->bus_type, driver);
            return NULL;
        }
    }

    if (qdev_should_hide_device(opts, from_json, errp)) {
        if (bus && !qbus_is_hotpluggable(bus)) {
            error_setg(errp, "Bus '%s' does not support hotplugging",
                       bus->name);
        }
        return NULL;
    } else if (*errp) {
        return NULL;
    }

    dev = qdev_new(driver);

    if (phase_check(PHASE_MACHINE_READY) &&
        !qdev_hotplug_allowed(dev, bus, errp)) {
        goto err_del_dev;
    }

    id = g_strdup(qdict_get_try_str(opts, "id"));
    if (!qdev_set_id(dev, id, errp)) {
        goto err_del_dev;
    }

    properties = qdict_clone_shallow(opts);
    qdict_del(properties, "driver");
    qdict_del(properties, "bus");
    qdict_del(properties, "id");

    object_set_properties_from_keyval(&dev->parent_obj, properties, from_json,
                                      errp);
    qobject_unref(properties);
    if (*errp) {
        goto err_del_dev;
    }

    if (!qdev_realize(dev, bus, errp)) {
        goto err_del_dev;
    }
    return dev;

err_del_dev:
    if (dev) {
        object_unparent(OBJECT(dev));
        object_unref(OBJECT(dev));
    }
    return NULL;
}

DeviceState *qdev_device_add(QemuOpts *opts, Error **errp)
{
    QDict *qdict = qemu_opts_to_qdict(opts, NULL);
    DeviceState *ret;

    ret = qdev_device_add_from_qdict(qdict, false, errp);
    if (ret) {
        qemu_opts_del(opts);
    }
    qobject_unref(qdict);
    return ret;
}

void qmp_device_add(QDict *qdict, QObject **ret_data, Error **errp)
{
    DeviceState *dev;

    dev = qdev_device_add_from_qdict(qdict, true, errp);
    if (!dev) {
        drain_call_rcu();
    }
    object_unref(OBJECT(dev));
}

static DeviceState *find_device_state(const char *id, bool use_generic_error,
                                      Error **errp)
{
    Object *obj = object_resolve_path_at(qdev_get_peripheral(), id);
    DeviceState *dev;

    if (!obj) {
        error_set(errp,
                  (use_generic_error ?
                   ERROR_CLASS_GENERIC_ERROR : ERROR_CLASS_DEVICE_NOT_FOUND),
                  "Device '%s' not found", id);
        return NULL;
    }

    dev = (DeviceState *)object_dynamic_cast(obj, TYPE_DEVICE);
    if (!dev) {
        error_setg(errp, "%s is not a device", id);
        return NULL;
    }

    return dev;
}

void qdev_unplug(DeviceState *dev, Error **errp)
{
    HotplugHandler *hotplug_ctrl;
    HotplugHandlerClass *hdc;
    Error *local_err = NULL;

    if (!qdev_hotunplug_allowed(dev, errp)) {
        return;
    }

    qdev_hot_removed = true;

    hotplug_ctrl = qdev_get_hotplug_handler(dev);
    g_assert(hotplug_ctrl);

    hdc = HOTPLUG_HANDLER_GET_CLASS(hotplug_ctrl);
    if (hdc->unplug_request) {
        hotplug_handler_unplug_request(hotplug_ctrl, dev, &local_err);
    } else {
        hotplug_handler_unplug(hotplug_ctrl, dev, &local_err);
        if (!local_err) {
            object_unparent(OBJECT(dev));
        }
    }
    error_propagate(errp, local_err);
}

void qmp_device_del(const char *id, Error **errp)
{
    DeviceState *dev = find_device_state(id, false, errp);
    if (dev != NULL) {
        if (dev->pending_deleted_event &&
            (dev->pending_deleted_expires_ms == 0 ||
             dev->pending_deleted_expires_ms > qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL))) {
            error_setg(errp, "Device %s is already in the "
                             "process of unplug", id);
            return;
        }

        qdev_unplug(dev, errp);
    }
}

int qdev_sync_config(DeviceState *dev, Error **errp)
{
    DeviceClass *dc = DEVICE_GET_CLASS(dev);

    if (!dc->sync_config) {
        error_setg(errp, "device-sync-config is not supported for '%s'",
                   object_get_typename(OBJECT(dev)));
        return -ENOTSUP;
    }

    return dc->sync_config(dev, errp);
}

void qmp_device_sync_config(const char *id, Error **errp)
{
    DeviceState *dev;

    dev = find_device_state(id, true, errp);
    if (!dev) {
        return;
    }

    qdev_sync_config(dev, errp);
}

BlockBackend *blk_by_qdev_id(const char *id, Error **errp)
{
    DeviceState *dev;
    BlockBackend *blk;

    GLOBAL_STATE_CODE();

    dev = find_device_state(id, false, errp);
    if (dev == NULL) {
        return NULL;
    }

    blk = blk_by_dev(dev);
    if (!blk) {
        error_setg(errp, "Device does not have a block device backend");
    }
    return blk;
}

QemuOptsList qemu_device_opts = {
    .name = "device",
    .implied_opt_name = "driver",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_device_opts.head),
    .desc = {
        { /* end of list */ }
    },
};

QemuOptsList qemu_global_opts = {
    .name = "global",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_global_opts.head),
    .desc = {
        {
            .name = "driver",
            .type = QEMU_OPT_STRING,
        },{
            .name = "property",
            .type = QEMU_OPT_STRING,
        },{
            .name = "value",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

int qemu_global_option(const char *str)
{
    char driver[64], property[64];
    QemuOpts *opts;
    int rc, offset;

    rc = sscanf(str, "%63[^.=].%63[^=]%n", driver, property, &offset);
    if (rc == 2 && str[offset] == '=') {
        opts = qemu_opts_create(&qemu_global_opts, NULL, 0, &error_abort);
        qemu_opt_set(opts, "driver", driver, &error_abort);
        qemu_opt_set(opts, "property", property, &error_abort);
        qemu_opt_set(opts, "value", str + offset + 1, &error_abort);
        return 0;
    }

    opts = qemu_opts_parse_noisily(&qemu_global_opts, str, false);
    if (!opts) {
        return -1;
    }
    if (!qemu_opt_get(opts, "driver")
        || !qemu_opt_get(opts, "property")
        || !qemu_opt_get(opts, "value")) {
        error_report("options 'driver', 'property', and 'value'"
                     " are required");
        return -1;
    }

    return 0;
}
