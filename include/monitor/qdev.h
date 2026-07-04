#ifndef MONITOR_QDEV_H
#define MONITOR_QDEV_H

#include "qemu/readline.h"

void hmp_info_qtree(Monitor *mon, const QDict *qdict);
void hmp_info_qdm(Monitor *mon, const QDict *qdict);
void hmp_device_add(Monitor *mon, const QDict *qdict);
void hmp_device_del(Monitor *mon, const QDict *qdict);
void device_add_completion(ReadLineState *rs, int nb_args, const char *str);
void device_del_completion(ReadLineState *rs, int nb_args, const char *str);
void qmp_device_add(QDict *qdict, QObject **ret_data, Error **errp);

int qdev_device_help(QemuOpts *opts);
DeviceState *qdev_device_add(QemuOpts *opts, Error **errp);
DeviceState *qdev_device_add_from_qdict(const QDict *opts,
                                        bool from_json, Error **errp);

const char *qdev_set_id(DeviceState *dev, char *id, Error **errp);

#endif
