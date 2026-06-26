
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/option.h"
#include "chardev/char.h"
#include "chardev-internal.h"


static int hub_chr_write(Chardev *chr, const uint8_t *buf, int len)
{
    HubChardev *d = HUB_CHARDEV(chr);
    int r, i, ret = len;
    unsigned int written;

    d->be_eagain_ind = -1;

    for (i = 0; i < d->be_cnt; i++) {
        if (!d->backends[i].be.chr->be_open) {
            continue;
        }
        written = d->be_written[i] - d->be_min_written;
        if (written) {
            ret = MIN(written, ret);
            continue;
        }
        r = qemu_chr_fe_write(&d->backends[i].be, buf, len);
        if (r < 0) {
            if (errno == EAGAIN) {
                d->be_eagain_ind = i;
            }
            return r;
        }
        d->be_written[i] += r;
        ret = MIN(r, ret);
    }
    d->be_min_written += ret;


    return ret;
}

static int hub_chr_can_read(void *opaque)
{
    HubCharBackend *backend = opaque;
    CharBackend *fe = backend->hub->parent.be;

    if (fe && fe->chr_can_read) {
        return fe->chr_can_read(fe->opaque);
    }

    return 0;
}

static void hub_chr_read(void *opaque, const uint8_t *buf, int size)
{
    HubCharBackend *backend = opaque;
    CharBackend *fe = backend->hub->parent.be;

    if (fe && fe->chr_read) {
        fe->chr_read(fe->opaque, buf, size);
    }
}

static void hub_chr_event(void *opaque, QEMUChrEvent event)
{
    HubCharBackend *backend = opaque;
    HubChardev *d = backend->hub;
    CharBackend *fe = d->parent.be;

    if (event == CHR_EVENT_OPENED) {
        d->be_written[backend->be_ind] = d->be_min_written;

        if (d->be_event_opened_cnt++) {
            return;
        }
    } else if (event == CHR_EVENT_CLOSED) {
        if (!d->be_event_opened_cnt) {
            return;
        }
        if (--d->be_event_opened_cnt) {
            return;
        }
    }

    if (fe && fe->chr_event) {
        fe->chr_event(fe->opaque, event);
    }
}

static GSource *hub_chr_add_watch(Chardev *s, GIOCondition cond)
{
    HubChardev *d = HUB_CHARDEV(s);
    Chardev *chr;
    ChardevClass *cc;

    if (d->be_eagain_ind == -1) {
        return NULL;
    }

    assert(d->be_eagain_ind < d->be_cnt);
    chr = qemu_chr_fe_get_driver(&d->backends[d->be_eagain_ind].be);
    cc = CHARDEV_GET_CLASS(chr);
    if (!cc->chr_add_watch) {
        return NULL;
    }

    return cc->chr_add_watch(chr, cond);
}

static bool hub_chr_attach_chardev(HubChardev *d, Chardev *chr,
                                   Error **errp)
{
    bool ret;

    if (d->be_cnt >= MAX_HUB) {
        error_setg(errp, "hub: too many uses of chardevs '%s'"
                   " (maximum is " stringify(MAX_HUB) ")",
                   d->parent.label);
        return false;
    }
    ret = qemu_chr_fe_init(&d->backends[d->be_cnt].be, chr, errp);
    if (ret) {
        d->backends[d->be_cnt].hub = d;
        d->backends[d->be_cnt].be_ind = d->be_cnt;
        d->be_cnt += 1;
    }

    return ret;
}

static void char_hub_finalize(Object *obj)
{
    HubChardev *d = HUB_CHARDEV(obj);
    int i;

    for (i = 0; i < d->be_cnt; i++) {
        qemu_chr_fe_deinit(&d->backends[i].be, false);
    }
}

static void hub_chr_update_read_handlers(Chardev *chr)
{
    HubChardev *d = HUB_CHARDEV(chr);
    int i;

    for (i = 0; i < d->be_cnt; i++) {
        qemu_chr_fe_set_handlers_full(&d->backends[i].be,
                                      hub_chr_can_read,
                                      hub_chr_read,
                                      hub_chr_event,
                                      NULL,
                                      &d->backends[i],
                                      chr->gcontext, true, false);
    }
}

static void qemu_chr_open_hub(Chardev *chr,
                                 ChardevBackend *backend,
                                 bool *be_opened,
                                 Error **errp)
{
    ChardevHub *hub = backend->u.hub.data;
    HubChardev *d = HUB_CHARDEV(chr);
    strList *list = hub->chardevs;

    d->be_eagain_ind = -1;

    if (list == NULL) {
        error_setg(errp, "hub: 'chardevs' list is not defined");
        return;
    }

    while (list) {
        Chardev *s;

        s = qemu_chr_find(list->value);
        if (s == NULL) {
            error_setg(errp, "hub: chardev can't be found by id '%s'",
                       list->value);
            return;
        }
        if (CHARDEV_IS_HUB(s) || CHARDEV_IS_MUX(s)) {
            error_setg(errp, "hub: multiplexers and hub devices can't be "
                       "stacked, check chardev '%s', chardev should not "
                       "be a hub device or have 'mux=on' enabled",
                       list->value);
            return;
        }
        if (!hub_chr_attach_chardev(d, s, errp)) {
            return;
        }
        list = list->next;
    }

    *be_opened = false;
}

static void qemu_chr_parse_hub(QemuOpts *opts, ChardevBackend *backend,
                                  Error **errp)
{
    ChardevHub *hub;
    strList **tail;
    int i;

    backend->type = CHARDEV_BACKEND_KIND_HUB;
    hub = backend->u.hub.data = g_new0(ChardevHub, 1);
    qemu_chr_parse_common(opts, qapi_ChardevHub_base(hub));

    tail = &hub->chardevs;

    for (i = 0; i < MAX_HUB; i++) {
        char optbuf[16];
        const char *dev;

        snprintf(optbuf, sizeof(optbuf), "chardevs.%u", i);
        dev = qemu_opt_get(opts, optbuf);
        if (!dev) {
            break;
        }

        QAPI_LIST_APPEND(tail, g_strdup(dev));
    }
}

static void char_hub_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->parse = qemu_chr_parse_hub;
    cc->open = qemu_chr_open_hub;
    cc->chr_write = hub_chr_write;
    cc->chr_add_watch = hub_chr_add_watch;
    cc->chr_be_event = NULL;
    cc->chr_update_read_handler = hub_chr_update_read_handlers;
}

static const TypeInfo char_hub_type_info = {
    .name = TYPE_CHARDEV_HUB,
    .parent = TYPE_CHARDEV,
    .class_init = char_hub_class_init,
    .instance_size = sizeof(HubChardev),
    .instance_finalize = char_hub_finalize,
};

static void register_types(void)
{
    type_register_static(&char_hub_type_info);
}

type_init(register_types);
