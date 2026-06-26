#ifndef MONITOR_QDEV_H
#define MONITOR_QDEV_H


void hmp_info_qtree(Monitor *mon, const QDict *qdict);
void hmp_info_qdm(Monitor *mon, const QDict *qdict);
void qmp_device_add(QDict *qdict, QObject **ret_data, Error **errp);

int qdev_device_help(QemuOpts *opts);
DeviceState *qdev_device_add(QemuOpts *opts, Error **errp);
DeviceState *qdev_device_add_from_qdict(const QDict *opts,
                                        bool from_json, Error **errp);

const char *qdev_set_id(DeviceState *dev, char *id, Error **errp);

#endif
