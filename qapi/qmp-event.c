
#include "qemu/osdep.h"

#include "qapi/qmp-event.h"
#include "qobject/qstring.h"
#include "qobject/qdict.h"
#include "qobject/qjson.h"

static void timestamp_put(QDict *qdict)
{
    QDict *ts;
    int64_t rt = g_get_real_time();

    ts = qdict_from_jsonf_nofail("{ 'seconds': %lld, 'microseconds': %lld }",
                                 (long long)rt / G_USEC_PER_SEC,
                                 (long long)rt % G_USEC_PER_SEC);
    qdict_put(qdict, "timestamp", ts);
}

QDict *qmp_event_build_dict(const char *event_name)
{
    QDict *dict = qdict_new();
    qdict_put_str(dict, "event", event_name);
    timestamp_put(dict);
    return dict;
}
