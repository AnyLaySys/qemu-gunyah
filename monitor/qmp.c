
#include "qemu/osdep.h"

#include "chardev/char-io.h"
#include "monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-control.h"
#include "qobject/qdict.h"
#include "qobject/qjson.h"
#include "qobject/qlist.h"
#include "trace.h"

static bool qmp_dispatcher_co_busy = true;

struct QMPRequest {
    MonitorQMP *mon;
    QObject *req;
    Error *err;
};
typedef struct QMPRequest QMPRequest;

QmpCommandList qmp_commands, qmp_cap_negotiation_commands;

static bool qmp_oob_enabled(MonitorQMP *mon)
{
    return mon->capab[QMP_CAPABILITY_OOB];
}

static void monitor_qmp_caps_reset(MonitorQMP *mon)
{
    memset(mon->capab_offered, 0, sizeof(mon->capab_offered));
    memset(mon->capab, 0, sizeof(mon->capab));
    mon->capab_offered[QMP_CAPABILITY_OOB] = mon->common.use_io_thread;
}

static void qmp_request_free(QMPRequest *req)
{
    qobject_unref(req->req);
    error_free(req->err);
    g_free(req);
}

static void monitor_qmp_cleanup_req_queue_locked(MonitorQMP *mon)
{
    while (!g_queue_is_empty(mon->qmp_requests)) {
        qmp_request_free(g_queue_pop_head(mon->qmp_requests));
    }
}

static void monitor_qmp_cleanup_queue_and_resume(MonitorQMP *mon)
{
    QEMU_LOCK_GUARD(&mon->qmp_queue_lock);

    bool need_resume = (!qmp_oob_enabled(mon) ||
        mon->qmp_requests->length == QMP_REQ_QUEUE_LEN_MAX)
        && !g_queue_is_empty(mon->qmp_requests);

    monitor_qmp_cleanup_req_queue_locked(mon);

    if (need_resume) {
        monitor_resume(&mon->common);
    }

}

void qmp_send_response(MonitorQMP *mon, const QDict *rsp)
{
    const QObject *data = QOBJECT(rsp);
    GString *json;

    json = qobject_to_json_pretty(data, mon->pretty);
    assert(json != NULL);
    trace_monitor_qmp_respond(mon, json->str);

    g_string_append_c(json, '\n');
    monitor_puts(&mon->common, json->str);

    g_string_free(json, true);
}

static void monitor_qmp_respond(MonitorQMP *mon, QDict *rsp)
{
    if (rsp) {
        qmp_send_response(mon, rsp);
    }
}

static void monitor_qmp_dispatch(MonitorQMP *mon, QObject *req)
{
    QDict *rsp;
    QDict *error;

    rsp = qmp_dispatch(mon->commands, req, qmp_oob_enabled(mon),
                       &mon->common);

    if (mon->commands == &qmp_cap_negotiation_commands) {
        error = qdict_get_qdict(rsp, "error");
        if (error
            && !g_strcmp0(qdict_get_try_str(error, "class"),
                    QapiErrorClass_str(ERROR_CLASS_COMMAND_NOT_FOUND))) {
            qdict_del(error, "desc");
            qdict_put_str(error, "desc", "Expecting capabilities negotiation"
                          " with 'qmp_capabilities'");
        }
    }

    monitor_qmp_respond(mon, rsp);
    qobject_unref(rsp);
}

static QMPRequest *monitor_qmp_requests_pop_any_with_lock(void)
{
    QMPRequest *req_obj = NULL;
    Monitor *mon;
    MonitorQMP *qmp_mon;

    QTAILQ_FOREACH(mon, &mon_list, entry) {
        if (!monitor_is_qmp(mon)) {
            continue;
        }

        qmp_mon = container_of(mon, MonitorQMP, common);
        qemu_mutex_lock(&qmp_mon->qmp_queue_lock);
        req_obj = g_queue_pop_head(qmp_mon->qmp_requests);
        if (req_obj) {
            break;
        }
        qemu_mutex_unlock(&qmp_mon->qmp_queue_lock);
    }

    if (req_obj) {
        QTAILQ_REMOVE(&mon_list, mon, entry);
        QTAILQ_INSERT_TAIL(&mon_list, mon, entry);
    }

    return req_obj;
}

static QMPRequest *monitor_qmp_dispatcher_pop_any(void)
{
    while (true) {
        assert(qatomic_read(&qmp_dispatcher_co_busy) == true);

        qatomic_set_mb(&qmp_dispatcher_co_busy, false);

        WITH_QEMU_LOCK_GUARD(&monitor_lock) {
            QMPRequest *req_obj;

            if (qmp_dispatcher_co_shutdown) {
                return NULL;
            }

            req_obj = monitor_qmp_requests_pop_any_with_lock();
            if (req_obj) {
                return req_obj;
            }
        }

        qemu_coroutine_yield();
    }
}

void coroutine_fn monitor_qmp_dispatcher_co(void *data)
{
    QMPRequest *req_obj;
    QDict *rsp;
    bool oob_enabled;
    MonitorQMP *mon;

    while ((req_obj = monitor_qmp_dispatcher_pop_any()) != NULL) {
        trace_monitor_qmp_in_band_dequeue(req_obj,
                                          req_obj->mon->qmp_requests->length);


        mon = req_obj->mon;

        oob_enabled = qmp_oob_enabled(mon);
        if (oob_enabled
            && mon->qmp_requests->length == QMP_REQ_QUEUE_LEN_MAX - 1) {
            monitor_resume(&mon->common);
        }

        qemu_mutex_unlock(&mon->qmp_queue_lock);

        if (qatomic_xchg(&qmp_dispatcher_co_busy, true) == true) {
            qemu_coroutine_yield();
        }

        if (req_obj->req) {
            if (trace_event_get_state(TRACE_MONITOR_QMP_CMD_IN_BAND)) {
                QDict *qdict = qobject_to(QDict, req_obj->req);
                QObject *id = qdict ? qdict_get(qdict, "id") : NULL;
                GString *id_json;

                id_json = id ? qobject_to_json(id) : g_string_new(NULL);
                trace_monitor_qmp_cmd_in_band(id_json->str);
                g_string_free(id_json, true);
            }
            monitor_qmp_dispatch(mon, req_obj->req);
        } else {
            assert(req_obj->err);
            trace_monitor_qmp_err_in_band(error_get_pretty(req_obj->err));
            rsp = qmp_error_response(req_obj->err);
            req_obj->err = NULL;
            monitor_qmp_respond(mon, rsp);
            qobject_unref(rsp);
        }

        if (!oob_enabled) {
            monitor_resume(&mon->common);
        }

        qmp_request_free(req_obj);
    }
    qatomic_set(&qmp_dispatcher_co, NULL);
}

void qmp_dispatcher_co_wake(void)
{
    smp_mb__before_rmw();

    if (!qatomic_xchg(&qmp_dispatcher_co_busy, true)) {
        aio_co_wake(qmp_dispatcher_co);
    }
}

static void handle_qmp_command(void *opaque, QObject *req, Error *err)
{
    MonitorQMP *mon = opaque;
    QDict *qdict = qobject_to(QDict, req);
    QMPRequest *req_obj;

    assert(!req != !err);

    if (req && trace_event_get_state_backends(TRACE_HANDLE_QMP_COMMAND)) {
        GString *req_json = qobject_to_json(req);
        trace_handle_qmp_command(mon, req_json->str);
        g_string_free(req_json, true);
    }

    if (qdict && qmp_is_oob(qdict)) {
        if (trace_event_get_state(TRACE_MONITOR_QMP_CMD_OUT_OF_BAND)) {
            QObject *id = qdict_get(qdict, "id");
            GString *id_json;

            id_json = id ? qobject_to_json(id) : g_string_new(NULL);
            trace_monitor_qmp_cmd_out_of_band(id_json->str);
            g_string_free(id_json, true);
        }
        monitor_qmp_dispatch(mon, req);
        qobject_unref(req);
        return;
    }

    req_obj = g_new0(QMPRequest, 1);
    req_obj->mon = mon;
    req_obj->req = req;
    req_obj->err = err;

    WITH_QEMU_LOCK_GUARD(&mon->qmp_queue_lock) {

        if (!qmp_oob_enabled(mon) ||
            mon->qmp_requests->length == QMP_REQ_QUEUE_LEN_MAX - 1) {
            monitor_suspend(&mon->common);
        }

        trace_monitor_qmp_in_band_enqueue(req_obj, mon,
                                          mon->qmp_requests->length);
        assert(mon->qmp_requests->length < QMP_REQ_QUEUE_LEN_MAX);
        g_queue_push_tail(mon->qmp_requests, req_obj);
    }

    qmp_dispatcher_co_wake();
}

static void monitor_qmp_read(void *opaque, const uint8_t *buf, int size)
{
    MonitorQMP *mon = opaque;

    json_message_parser_feed(&mon->parser, (const char *) buf, size);
}

static QDict *qmp_greeting(MonitorQMP *mon)
{
    QList *cap_list = qlist_new();
    QObject *ver = NULL;
    QDict *args;
    QMPCapability cap;

    args = qdict_new();
    qmp_marshal_query_version(args, &ver, NULL);
    qobject_unref(args);

    for (cap = 0; cap < QMP_CAPABILITY__MAX; cap++) {
        if (mon->capab_offered[cap]) {
            qlist_append_str(cap_list, QMPCapability_str(cap));
        }
    }

    return qdict_from_jsonf_nofail(
        "{'QMP': {'version': %p, 'capabilities': %p}}",
        ver, cap_list);
}

static void monitor_qmp_event(void *opaque, QEMUChrEvent event)
{
    QDict *data;
    MonitorQMP *mon = opaque;

    switch (event) {
    case CHR_EVENT_OPENED:
        mon->commands = &qmp_cap_negotiation_commands;
        monitor_qmp_caps_reset(mon);
        data = qmp_greeting(mon);
        qmp_send_response(mon, data);
        qobject_unref(data);
        break;
    case CHR_EVENT_CLOSED:
        monitor_qmp_cleanup_queue_and_resume(mon);
        json_message_parser_destroy(&mon->parser);
        json_message_parser_init(&mon->parser, handle_qmp_command,
                                 mon, NULL);
        monitor_fdsets_cleanup();
        break;
    case CHR_EVENT_BREAK:
    case CHR_EVENT_MUX_IN:
    case CHR_EVENT_MUX_OUT:
        break;
    }
}

void monitor_data_destroy_qmp(MonitorQMP *mon)
{
    json_message_parser_destroy(&mon->parser);
    qemu_mutex_destroy(&mon->qmp_queue_lock);
    monitor_qmp_cleanup_req_queue_locked(mon);
    g_queue_free(mon->qmp_requests);
}

static void monitor_qmp_setup_handlers_bh(void *opaque)
{
    MonitorQMP *mon = opaque;
    GMainContext *context;

    assert(mon->common.use_io_thread);
    context = iothread_get_g_main_context(mon_iothread);
    assert(context);
    qemu_chr_fe_set_handlers(&mon->common.chr, monitor_can_read,
                             monitor_qmp_read, monitor_qmp_event,
                             NULL, &mon->common, context, true);
    monitor_list_append(&mon->common);
}

void monitor_init_qmp(Chardev *chr, bool pretty, Error **errp)
{
    MonitorQMP *mon = g_new0(MonitorQMP, 1);

    if (!qemu_chr_fe_init(&mon->common.chr, chr, errp)) {
        g_free(mon);
        return;
    }
    qemu_chr_fe_set_echo(&mon->common.chr, true);

    monitor_data_init(&mon->common, true, false,
                      qemu_chr_has_feature(chr, QEMU_CHAR_FEATURE_GCONTEXT));

    mon->pretty = pretty;

    qemu_mutex_init(&mon->qmp_queue_lock);
    mon->qmp_requests = g_queue_new();

    json_message_parser_init(&mon->parser, handle_qmp_command, mon, NULL);
    if (mon->common.use_io_thread) {
        remove_fd_in_watch(chr);
        aio_bh_schedule_oneshot(iothread_get_aio_context(mon_iothread),
                                monitor_qmp_setup_handlers_bh, mon);
    } else {
        qemu_chr_fe_set_handlers(&mon->common.chr, monitor_can_read,
                                 monitor_qmp_read, monitor_qmp_event,
                                 NULL, &mon->common, NULL, true);
        monitor_list_append(&mon->common);
    }
}
