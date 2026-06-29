
#include "qemu/osdep.h"
#include "monitor-internal.h"
#include "monitor/hmp.h"
#include "qemu/cutils.h"

static HMPCommand hmp_info_cmds[];

int hmp_compare_cmd(const char *name, const char *list)
{
    const char *p, *pstart;
    int len;
    len = strlen(name);
    p = list;
    for (;;) {
        pstart = p;
        p = qemu_strchrnul(p, '|');
        if ((p - pstart) == len && !memcmp(pstart, name, len)) {
            return 1;
        }
        if (*p == '\0') {
            break;
        }
        p++;
    }
    return 0;
}

static HMPCommand hmp_info_cmds[] = {
#include "hmp-commands-info.h"
    { NULL, NULL, },
};

HMPCommand hmp_cmds[] = {
#include "hmp-commands.h"
    { NULL, NULL, },
};

int get_monitor_def(Monitor *mon, int64_t *pval, const char *name)
{
    return -1;
}

static int
compare_mon_cmd(const void *a, const void *b)
{
    return strcmp(((const HMPCommand *)a)->name,
            ((const HMPCommand *)b)->name);
}

static void __attribute__((__constructor__)) sortcmdlist(void)
{
    qsort(hmp_cmds, ARRAY_SIZE(hmp_cmds) - 1,
          sizeof(*hmp_cmds),
          compare_mon_cmd);
    qsort(hmp_info_cmds, ARRAY_SIZE(hmp_info_cmds) - 1,
          sizeof(*hmp_info_cmds),
          compare_mon_cmd);
}

void monitor_register_hmp(const char *name, bool info,
                          void (*cmd)(Monitor *mon, const QDict *qdict))
{
    HMPCommand *table = info ? hmp_info_cmds : hmp_cmds;

    while (table->name != NULL) {
        if (strcmp(table->name, name) == 0) {
            g_assert(table->cmd == NULL && table->cmd_info_hrt == NULL);
            table->cmd = cmd;
            return;
        }
        table++;
    }
    g_assert_not_reached();
}

void monitor_register_hmp_info_hrt(const char *name,
                                   HumanReadableText *(*handler)(Error **errp))
{
    HMPCommand *table = hmp_info_cmds;

    while (table->name != NULL) {
        if (strcmp(table->name, name) == 0) {
            g_assert(table->cmd == NULL && table->cmd_info_hrt == NULL);
            table->cmd_info_hrt = handler;
            return;
        }
        table++;
    }
    g_assert_not_reached();
}
