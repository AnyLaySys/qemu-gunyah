#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "monitor/hmp.h"
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-machine.h"
#include "qapi/qapi-commands-control.h"
#include "qapi/qapi-commands-misc.h"
#include "qobject/qdict.h"
#include "ui/input.h"

bool hmp_handle_error(Monitor *mon, Error *err)
{
    if (err) {
        error_reportf_err(err, "Error: ");
        return true;
    }
    return false;
}

void hmp_info_name(Monitor *mon, const QDict *qdict)
{
    NameInfo *info = qmp_query_name(NULL);
    if (info->name) {
        monitor_printf(mon, "%s\n", info->name);
    }
    qapi_free_NameInfo(info);
}

void hmp_info_version(Monitor *mon, const QDict *qdict)
{
    VersionInfo *info = qmp_query_version(NULL);
    monitor_printf(mon, "%" PRId64 ".%" PRId64 ".%" PRId64 "%s\n",
                   info->qemu->major, info->qemu->minor, info->qemu->micro,
                   info->package);
    qapi_free_VersionInfo(info);
}

void hmp_quit(Monitor *mon, const QDict *qdict)
{
    monitor_suspend(mon);
    qmp_quit(NULL);
}

void hmp_stop(Monitor *mon, const QDict *qdict)
{
    qmp_stop(NULL);
}

void hmp_cont(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    qmp_cont(&err);
    hmp_handle_error(mon, err);
}

void hmp_system_reset(Monitor *mon, const QDict *qdict)
{
    qmp_system_reset(NULL);
}

static int hmp_key_index(const char *key, size_t key_length)
{
    int i;

    for (i = 0; i < Q_KEY_CODE__MAX; i++) {
        if (!strncmp(key, QKeyCode_str(i), key_length) &&
            !QKeyCode_str(i)[key_length]) {
            break;
        }
    }
    return i;
}

void hmp_sendkey(Monitor *mon, const QDict *qdict)
{
    const char *keys = qdict_get_str(qdict, "keys");
    int64_t hold_time = qdict_get_try_int(qdict, "hold-time", 0);
    QKeyCode *keycodes = NULL;
    size_t count = 0;
    const char *separator;
    int keyname_len;

    while (true) {
        int qcode;

        separator = qemu_strchrnul(keys, '-');
        keyname_len = separator - keys;
        if (keys[0] == '<' && keyname_len == 1) {
            keys = "less";
            keyname_len = 4;
        }
        if (strstart(keys, "0x", NULL)) {
            const char *endp;
            int value;

            if (qemu_strtoi(keys, &endp, 0, &value) < 0 ||
                endp != keys + keyname_len) {
                goto invalid;
            }
            qcode = qemu_input_key_number_to_qcode(value);
        } else {
            qcode = hmp_key_index(keys, keyname_len);
            if (qcode == Q_KEY_CODE__MAX) {
                goto invalid;
            }
        }
        keycodes = g_renew(QKeyCode, keycodes, count + 1);
        keycodes[count++] = qcode;
        if (!*separator) {
            break;
        }
        keys = separator + 1;
    }

    for (size_t i = 0; i < count; i++) {
        qemu_input_event_send_key_qcode(NULL, keycodes[i], true);
        qemu_input_event_send_key_delay(hold_time);
    }
    while (count) {
        count--;
        qemu_input_event_send_key_qcode(NULL, keycodes[count], false);
        qemu_input_event_send_key_delay(hold_time);
    }
    g_free(keycodes);
    return;

invalid:
    monitor_printf(mon, "invalid parameter: %.*s\n", keyname_len, keys);
    g_free(keycodes);
}

void hmp_help(Monitor *mon, const QDict *qdict)
{
    hmp_help_cmd(mon, qdict_get_try_str(qdict, "name"));
}

void hmp_info_help(Monitor *mon, const QDict *qdict)
{
    hmp_help_cmd(mon, "info");
}
