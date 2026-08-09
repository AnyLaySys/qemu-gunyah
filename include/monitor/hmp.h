#ifndef HMP_H
#define HMP_H

typedef struct Error Error;
typedef struct HumanReadableText HumanReadableText;
typedef struct Monitor Monitor;
typedef struct QDict QDict;

bool hmp_handle_error(Monitor *mon, Error *err);
void hmp_help_cmd(Monitor *mon, const char *name);
void hmp_info_name(Monitor *mon, const QDict *qdict);
void hmp_info_version(Monitor *mon, const QDict *qdict);
void hmp_info_status(Monitor *mon, const QDict *qdict);
void hmp_quit(Monitor *mon, const QDict *qdict);
void hmp_stop(Monitor *mon, const QDict *qdict);
void hmp_cont(Monitor *mon, const QDict *qdict);
void hmp_system_reset(Monitor *mon, const QDict *qdict);
void hmp_sendkey(Monitor *mon, const QDict *qdict);
void hmp_help(Monitor *mon, const QDict *qdict);
void hmp_info_help(Monitor *mon, const QDict *qdict);
void hmp_human_readable_text_helper(Monitor *mon,
                                    HumanReadableText *(*qmp_handler)(Error **));

#endif
