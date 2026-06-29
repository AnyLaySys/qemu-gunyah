
#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "monitor/monitor.h"
#include "monitor/qmp-helpers.h"
#include "qemu/config-file.h"
#include "qemu/error-report.h"
#include "qemu/qemu-print.h"
#include "chardev/char.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-char.h"
#include "qapi/qmp/qerror.h"
#include "qemu/help_option.h"
#include "qemu/module.h"
#include "qemu/option.h"
#include "qemu/id.h"
#include "qemu/coroutine.h"
#include "qemu/yank.h"

#include "chardev-internal.h"


Object *get_chardevs_root(void)
{
    return object_get_container("chardevs");
}

static void chr_be_event(Chardev *s, QEMUChrEvent event)
{
    CharBackend *be = s->be;

    if (!be || !be->chr_event) {
        return;
    }

    be->chr_event(be->opaque, event);
}

void qemu_chr_be_event(Chardev *s, QEMUChrEvent event)
{
    switch (event) {
        case CHR_EVENT_OPENED:
            s->be_open = 1;
            break;
        case CHR_EVENT_CLOSED:
            s->be_open = 0;
            break;
    case CHR_EVENT_BREAK:
    case CHR_EVENT_MUX_IN:
    case CHR_EVENT_MUX_OUT:
        break;
    }

    CHARDEV_GET_CLASS(s)->chr_be_event(s, event);
}

static void qemu_chr_write_log(Chardev *s, const uint8_t *buf, size_t len)
{
    size_t done = 0;
    ssize_t ret;

    if (s->logfd < 0) {
        return;
    }

    while (done < len) {
    retry:
        ret = write(s->logfd, buf + done, len - done);
        if (ret == -1 && errno == EAGAIN) {
            g_usleep(100);
            goto retry;
        }

        if (ret <= 0) {
            return;
        }
        done += ret;
    }
}

static int qemu_chr_write_buffer(Chardev *s,
                                 const uint8_t *buf, int len,
                                 int *offset, bool write_all)
{
    ChardevClass *cc = CHARDEV_GET_CLASS(s);
    int res = 0;
    *offset = 0;

    qemu_mutex_lock(&s->chr_write_lock);
    while (*offset < len) {
    retry:
        res = cc->chr_write(s, buf + *offset, len - *offset);
        if (res < 0 && errno == EAGAIN && write_all) {
            if (qemu_in_coroutine()) {
                qemu_co_sleep_ns(QEMU_CLOCK_REALTIME, 100000);
            } else {
                g_usleep(100);
            }
            goto retry;
        }

        if (res <= 0) {
            break;
        }

        *offset += res;
        if (!write_all) {
            break;
        }
    }
    if (*offset > 0) {
        qemu_chr_write_log(s, buf, *offset);
    } else if (res < 0) {
        qemu_chr_write_log(s, buf, len);
    }
    qemu_mutex_unlock(&s->chr_write_lock);

    return res;
}

int qemu_chr_write(Chardev *s, const uint8_t *buf, int len, bool write_all)
{
    int offset = 0;
    int res;

    res = qemu_chr_write_buffer(s, buf, len, &offset, write_all);

    if (res < 0) {
        return res;
    }
    return offset;
}

int qemu_chr_be_can_write(Chardev *s)
{
    CharBackend *be = s->be;

    if (!be || !be->chr_can_read) {
        return 0;
    }

    return be->chr_can_read(be->opaque);
}

void qemu_chr_be_write_impl(Chardev *s, const uint8_t *buf, int len)
{
    CharBackend *be = s->be;

    if (be && be->chr_read) {
        be->chr_read(be->opaque, buf, len);
    }
}

void qemu_chr_be_write(Chardev *s, const uint8_t *buf, int len)
{
    qemu_chr_be_write_impl(s, buf, len);
}

void qemu_chr_be_update_read_handlers(Chardev *s,
                                      GMainContext *context)
{
    ChardevClass *cc = CHARDEV_GET_CLASS(s);

    assert(qemu_chr_has_feature(s, QEMU_CHAR_FEATURE_GCONTEXT)
           || !context);
    s->gcontext = context;
    if (cc->chr_update_read_handler) {
        cc->chr_update_read_handler(s);
    }
}

static void qemu_char_open(Chardev *chr, ChardevBackend *backend,
                           bool *be_opened, Error **errp)
{
    ChardevClass *cc = CHARDEV_GET_CLASS(chr);
    ChardevCommon *common = backend ? backend->u.null.data : NULL;

    if (common && common->logfile) {
        int flags = O_WRONLY;
        if (common->has_logappend &&
            common->logappend) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        chr->logfd = qemu_create(common->logfile, flags, 0666, errp);
        if (chr->logfd < 0) {
            return;
        }
    }

    if (cc->open) {
        cc->open(chr, backend, be_opened, errp);
    }
}

static void char_init(Object *obj)
{
    Chardev *chr = CHARDEV(obj);

    chr->handover_yank_instance = false;
    chr->logfd = -1;
    qemu_mutex_init(&chr->chr_write_lock);

    if (CHARDEV_GET_CLASS(chr)->chr_update_read_handler) {
        qemu_chr_set_feature(chr, QEMU_CHAR_FEATURE_GCONTEXT);
    }

}

static int null_chr_write(Chardev *chr, const uint8_t *buf, int len)
{
    return len;
}

static void char_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->chr_write = null_chr_write;
    cc->chr_be_event = chr_be_event;
}

static void char_finalize(Object *obj)
{
    Chardev *chr = CHARDEV(obj);

    if (chr->be) {
        chr->be->chr = NULL;
    }
    g_free(chr->filename);
    g_free(chr->label);
    if (chr->logfd != -1) {
        close(chr->logfd);
    }
    qemu_mutex_destroy(&chr->chr_write_lock);
}

static const TypeInfo char_type_info = {
    .name = TYPE_CHARDEV,
    .parent = TYPE_OBJECT,
    .instance_size = sizeof(Chardev),
    .instance_init = char_init,
    .instance_finalize = char_finalize,
    .abstract = true,
    .class_size = sizeof(ChardevClass),
    .class_init = char_class_init,
};

int qemu_chr_wait_connected(Chardev *chr, Error **errp)
{
    ChardevClass *cc = CHARDEV_GET_CLASS(chr);

    if (cc->chr_wait_connected) {
        return cc->chr_wait_connected(chr, errp);
    }

    return 0;
}

QemuOpts *qemu_chr_parse_compat(const char *label, const char *filename,
                                bool permit_mux_mon)
{
    const char *p;
    QemuOpts *opts;
    Error *local_err = NULL;

    opts = qemu_opts_create(qemu_find_opts("chardev"), label, 1, &local_err);
    if (local_err) {
        error_report_err(local_err);
        return NULL;
    }

    if (strstart(filename, "mon:", &p)) {
        if (!permit_mux_mon) {
            error_report("mon: isn't supported in this context");
            return NULL;
        }
        filename = p;
        qemu_opt_set(opts, "mux", "on", &error_abort);
        if (strcmp(filename, "stdio") == 0) {
            qemu_opt_set(opts, "signal", "off", &error_abort);
        }
    }

    if (strcmp(filename, "null") == 0 ||
        strcmp(filename, "stdio") == 0) {
        qemu_opt_set(opts, "backend", filename, &error_abort);
        return opts;
    }

    error_report("'%s' is not a valid char driver", filename);
    qemu_opts_del(opts);
    return NULL;
}

void qemu_chr_parse_common(QemuOpts *opts, ChardevCommon *backend)
{
    const char *logfile = qemu_opt_get(opts, "logfile");

    backend->logfile = g_strdup(logfile);
    backend->has_logappend = true;
    backend->logappend = qemu_opt_get_bool(opts, "logappend", false);
}

static const ChardevClass *char_get_class(const char *driver, Error **errp)
{
    ObjectClass *oc;
    const ChardevClass *cc;
    char *typename = g_strdup_printf("chardev-%s", driver);

    oc = module_object_class_by_name(typename);
    g_free(typename);

    if (!object_class_dynamic_cast(oc, TYPE_CHARDEV)) {
        error_setg(errp, "'%s' is not a valid char driver name", driver);
        return NULL;
    }

    if (object_class_is_abstract(oc)) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "driver",
                   "a non-abstract device type");
        return NULL;
    }

    cc = CHARDEV_CLASS(oc);
    if (cc->internal) {
        error_setg(errp, "'%s' is not a valid char driver name", driver);
        return NULL;
    }

    return cc;
}

typedef struct ChadevClassFE {
    void (*fn)(const char *name, void *opaque);
    void *opaque;
} ChadevClassFE;

static void
chardev_class_foreach(ObjectClass *klass, void *opaque)
{
    ChadevClassFE *fe = opaque;

    assert(g_str_has_prefix(object_class_get_name(klass), "chardev-"));
    if (CHARDEV_CLASS(klass)->internal) {
        return;
    }

    fe->fn(object_class_get_name(klass) + 8, fe->opaque);
}

static void
chardev_name_foreach(void (*fn)(const char *name, void *opaque),
                     void *opaque)
{
    ChadevClassFE fe = { .fn = fn, .opaque = opaque };

    object_class_foreach(chardev_class_foreach, TYPE_CHARDEV, false, &fe);
}

static void
help_string_append(const char *name, void *opaque)
{
    GString *str = opaque;

    g_string_append_printf(str, "\n  %s", name);
}

ChardevBackend *qemu_chr_parse_opts(QemuOpts *opts, Error **errp)
{
    Error *local_err = NULL;
    const ChardevClass *cc;
    ChardevBackend *backend = NULL;
    const char *name = qemu_opt_get(opts, "backend");

    if (name == NULL) {
        error_setg(errp, "chardev: \"%s\" missing backend",
                   qemu_opts_id(opts));
        return NULL;
    }

    cc = char_get_class(name, errp);
    if (cc == NULL) {
        return NULL;
    }

    backend = g_new0(ChardevBackend, 1);
    backend->type = CHARDEV_BACKEND_KIND_NULL;

    if (cc->parse) {
        cc->parse(opts, backend, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            qapi_free_ChardevBackend(backend);
            return NULL;
        }
    } else {
        ChardevCommon *ccom = g_new0(ChardevCommon, 1);
        qemu_chr_parse_common(opts, ccom);
        backend->u.null.data = ccom; /* Any ChardevCommon member would work */
    }

    return backend;
}

static Chardev *do_qemu_chr_new_from_opts(QemuOpts *opts, GMainContext *context,
                                          Error **errp)
{
    const ChardevClass *cc;
    Chardev *chr = NULL;
    ChardevBackend *backend = NULL;
    const char *name = qemu_opt_get(opts, "backend");
    const char *id = qemu_opts_id(opts);
    char *bid = NULL;

    if (name && is_help_option(name)) {
        GString *str = g_string_new("");

        chardev_name_foreach(help_string_append, str);

        qemu_printf("Available chardev backend types: %s\n", str->str);
        g_string_free(str, true);
        return NULL;
    }

    if (id == NULL) {
        error_setg(errp, "chardev: no id specified");
        return NULL;
    }

    backend = qemu_chr_parse_opts(opts, errp);
    if (backend == NULL) {
        return NULL;
    }

    cc = char_get_class(name, errp);
    if (cc == NULL) {
        goto out;
    }

    if (qemu_opt_get_bool(opts, "mux", 0)) {
        bid = g_strdup_printf("%s-base", id);
    }

    chr = qemu_chardev_new(bid ? bid : id,
                           object_class_get_name(OBJECT_CLASS(cc)),
                           backend, context, errp);
    if (chr == NULL) {
        goto out;
    }

    if (bid) {
        Chardev *mux;
        qapi_free_ChardevBackend(backend);
        backend = g_new0(ChardevBackend, 1);
        backend->type = CHARDEV_BACKEND_KIND_MUX;
        backend->u.mux.data = g_new0(ChardevMux, 1);
        backend->u.mux.data->chardev = g_strdup(bid);
        mux = qemu_chardev_new(id, TYPE_CHARDEV_MUX, backend, context, errp);
        if (mux == NULL) {
            object_unparent(OBJECT(chr));
            chr = NULL;
            goto out;
        }
        chr = mux;
    }

out:
    qapi_free_ChardevBackend(backend);
    g_free(bid);

    return chr;
}

Chardev *qemu_chr_new_from_opts(QemuOpts *opts, GMainContext *context,
                                Error **errp)
{
    return do_qemu_chr_new_from_opts(opts, context, errp);
}

static Chardev *qemu_chr_new_from_name(const char *label, const char *filename,
                                       bool permit_mux_mon,
                                       GMainContext *context)
{
    const char *p;
    Chardev *chr;
    QemuOpts *opts;
    Error *err = NULL;

    if (strstart(filename, "chardev:", &p)) {
        chr = qemu_chr_find(p);
        return chr;
    }

    opts = qemu_chr_parse_compat(label, filename, permit_mux_mon);
    if (!opts)
        return NULL;

    chr = do_qemu_chr_new_from_opts(opts, context, &err);
    if (!chr) {
        error_report_err(err);
        goto out;
    }

    if (qemu_opt_get_bool(opts, "mux", 0)) {
        assert(permit_mux_mon);
        monitor_init_hmp(chr, true, &err);
        if (err) {
            error_report_err(err);
            object_unparent(OBJECT(chr));
            chr = NULL;
            goto out;
        }
    }

out:
    qemu_opts_del(opts);
    return chr;
}

static Chardev *qemu_chr_new_permit_mux_mon(const char *label,
                                          const char *filename,
                                          bool permit_mux_mon,
                                          GMainContext *context)
{
    return qemu_chr_new_from_name(label, filename, permit_mux_mon, context);
}

Chardev *qemu_chr_new(const char *label, const char *filename,
                      GMainContext *context)
{
    return qemu_chr_new_permit_mux_mon(label, filename, false, context);
}

Chardev *qemu_chr_new_mux_mon(const char *label, const char *filename,
                              GMainContext *context)
{
    return qemu_chr_new_permit_mux_mon(label, filename, true, context);
}

Chardev *qemu_chr_find(const char *name)
{
    Object *obj = object_resolve_path_component(get_chardevs_root(), name);

    return obj ? CHARDEV(obj) : NULL;
}

QemuOptsList qemu_chardev_opts = {
    .name = "chardev",
    .implied_opt_name = "backend",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_chardev_opts.head),
    .desc = {
        {
            .name = "backend",
            .type = QEMU_OPT_STRING,
        },{
            .name = "mux",
            .type = QEMU_OPT_BOOL,
        },{
            .name = "signal",
            .type = QEMU_OPT_BOOL,
        },{
            .name = "chardev",
            .type = QEMU_OPT_STRING,
        },{
            .name = "logfile",
            .type = QEMU_OPT_STRING,
        },{
            .name = "logappend",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

bool qemu_chr_has_feature(Chardev *chr,
                          ChardevFeature feature)
{
    return test_bit(feature, chr->features);
}

void qemu_chr_set_feature(Chardev *chr,
                           ChardevFeature feature)
{
    return set_bit(feature, chr->features);
}

static Chardev *chardev_new(const char *id, const char *typename,
                            ChardevBackend *backend,
                            GMainContext *gcontext,
                            bool handover_yank_instance,
                            Error **errp)
{
    Object *obj;
    Chardev *chr = NULL;
    Error *local_err = NULL;
    bool be_opened = true;

    assert(g_str_has_prefix(typename, "chardev-"));
    assert(id);

    obj = object_new(typename);
    chr = CHARDEV(obj);
    chr->handover_yank_instance = handover_yank_instance;
    chr->label = g_strdup(id);
    chr->gcontext = gcontext;

    qemu_char_open(chr, backend, &be_opened, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        object_unref(obj);
        return NULL;
    }

    if (!chr->filename) {
        chr->filename = g_strdup(typename + 8);
    }
    if (be_opened) {
        qemu_chr_be_event(chr, CHR_EVENT_OPENED);
    }

    return chr;
}

Chardev *qemu_chardev_new(const char *id, const char *typename,
                          ChardevBackend *backend,
                          GMainContext *gcontext,
                          Error **errp)
{
    Chardev *chr;
    g_autofree char *genid = NULL;

    if (!id) {
        genid = id_generate(ID_CHR);
        id = genid;
    }

    chr = chardev_new(id, typename, backend, gcontext, false, errp);
    if (!chr) {
        return NULL;
    }

    if (!object_property_try_add_child(get_chardevs_root(), id, OBJECT(chr),
                                       errp)) {
        object_unref(OBJECT(chr));
        return NULL;
    }
    object_unref(OBJECT(chr));

    return chr;
}

GSource *qemu_chr_timeout_add_ms(Chardev *chr, guint ms,
                                 GSourceFunc func, void *private)
{
    GSource *source = g_timeout_source_new(ms);

    assert(func);
    g_source_set_callback(source, func, private, NULL);
    g_source_attach(source, chr->gcontext);

    return source;
}

void qemu_chr_cleanup(void)
{
    object_unparent(get_chardevs_root());
}

static void register_types(void)
{
    type_register_static(&char_type_info);
}

type_init(register_types);
