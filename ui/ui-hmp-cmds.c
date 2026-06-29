
#include "qemu/osdep.h"
#include "monitor/hmp.h"
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-ui.h"
#include "qobject/qdict.h"
#include "qemu/cutils.h"
#include "ui/console.h"
#include "ui/input.h"

static int mouse_button_state;

void hmp_mouse_move(Monitor *mon, const QDict *qdict)
{
    int dx, dy, dz, button;
    const char *dx_str = qdict_get_str(qdict, "dx_str");
    const char *dy_str = qdict_get_str(qdict, "dy_str");
    const char *dz_str = qdict_get_try_str(qdict, "dz_str");

    dx = strtol(dx_str, NULL, 0);
    dy = strtol(dy_str, NULL, 0);
    qemu_input_queue_rel(NULL, INPUT_AXIS_X, dx);
    qemu_input_queue_rel(NULL, INPUT_AXIS_Y, dy);

    if (dz_str) {
        dz = strtol(dz_str, NULL, 0);
        if (dz != 0) {
            button = (dz > 0) ? INPUT_BUTTON_WHEEL_UP : INPUT_BUTTON_WHEEL_DOWN;
            qemu_input_queue_btn(NULL, button, true);
            qemu_input_event_sync();
            qemu_input_queue_btn(NULL, button, false);
        }
    }
    qemu_input_event_sync();
}

void hmp_mouse_button(Monitor *mon, const QDict *qdict)
{
    static uint32_t bmap[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT]       = MOUSE_EVENT_LBUTTON,
        [INPUT_BUTTON_MIDDLE]     = MOUSE_EVENT_MBUTTON,
        [INPUT_BUTTON_RIGHT]      = MOUSE_EVENT_RBUTTON,
    };
    int button_state = qdict_get_int(qdict, "button_state");

    if (mouse_button_state == button_state) {
        return;
    }
    qemu_input_update_buttons(NULL, bmap, mouse_button_state, button_state);
    qemu_input_event_sync();
    mouse_button_state = button_state;
}

void hmp_mouse_set(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    qemu_mouse_set(qdict_get_int(qdict, "index"), &err);
    hmp_handle_error(mon, err);
}

void hmp_info_mice(Monitor *mon, const QDict *qdict)
{
    MouseInfoList *mice_list, *mouse;

    mice_list = qmp_query_mice(NULL);
    if (!mice_list) {
        monitor_printf(mon, "No mouse devices connected\n");
        return;
    }

    for (mouse = mice_list; mouse; mouse = mouse->next) {
        monitor_printf(mon, "%c Mouse #%" PRId64 ": %s%s\n",
                       mouse->value->current ? '*' : ' ',
                       mouse->value->index, mouse->value->name,
                       mouse->value->absolute ? " (absolute)" : "");
    }

    qapi_free_MouseInfoList(mice_list);
}

void hmp_sendkey(Monitor *mon, const QDict *qdict)
{
    const char *keys = qdict_get_str(qdict, "keys");
    KeyValue *v = NULL;
    KeyValueList *head = NULL, **tail = &head;
    int has_hold_time = qdict_haskey(qdict, "hold-time");
    int hold_time = qdict_get_try_int(qdict, "hold-time", -1);
    Error *err = NULL;
    const char *separator;
    int keyname_len;

    while (1) {
        separator = qemu_strchrnul(keys, '-');
        keyname_len = separator - keys;

        if (keys[0] == '<' && keyname_len == 1) {
            keys = "less";
            keyname_len = 4;
        }

        v = g_malloc0(sizeof(*v));

        if (strstart(keys, "0x", NULL)) {
            const char *endp;
            int value;

            if (qemu_strtoi(keys, &endp, 0, &value) < 0) {
                goto err_out;
            }
            assert(endp <= keys + keyname_len);
            if (endp != keys + keyname_len) {
                goto err_out;
            }
            v->type = KEY_VALUE_KIND_NUMBER;
            v->u.number.data = value;
        } else {
            int idx = index_from_key(keys, keyname_len);
            if (idx == Q_KEY_CODE__MAX) {
                goto err_out;
            }
            v->type = KEY_VALUE_KIND_QCODE;
            v->u.qcode.data = idx;
        }
        QAPI_LIST_APPEND(tail, v);
        v = NULL;

        if (!*separator) {
            break;
        }
        keys = separator + 1;
    }

    qmp_send_key(head, has_hold_time, hold_time, &err);
    hmp_handle_error(mon, err);

out:
    qapi_free_KeyValue(v);
    qapi_free_KeyValueList(head);
    return;

err_out:
    monitor_printf(mon, "invalid parameter: %.*s\n", keyname_len, keys);
    goto out;
}

void sendkey_completion(ReadLineState *rs, int nb_args, const char *str)
{
    int i;
    char *sep;
    size_t len;

    if (nb_args != 2) {
        return;
    }
    sep = strrchr(str, '-');
    if (sep) {
        str = sep + 1;
    }
    len = strlen(str);
    readline_set_completion_index(rs, len);
    for (i = 0; i < Q_KEY_CODE__MAX; i++) {
        if (!strncmp(str, QKeyCode_str(i), len)) {
            readline_add_completion(rs, QKeyCode_str(i));
        }
    }
}

#ifdef CONFIG_PIXMAN
void coroutine_fn
hmp_screendump(Monitor *mon, const QDict *qdict)
{
    const char *filename = qdict_get_str(qdict, "filename");
    const char *id = qdict_get_try_str(qdict, "device");
    int64_t head = qdict_get_try_int(qdict, "head", 0);
    const char *input_format  = qdict_get_try_str(qdict, "format");
    Error *err = NULL;
    ImageFormat format;

    format = qapi_enum_parse(&ImageFormat_lookup, input_format,
                              IMAGE_FORMAT_PPM, &err);
    if (err) {
        goto end;
    }

    qmp_screendump(filename, id, id != NULL, head,
                   input_format != NULL, format, &err);
end:
    hmp_handle_error(mon, err);
}
#endif
