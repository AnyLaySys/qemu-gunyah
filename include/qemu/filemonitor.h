
#ifndef QEMU_FILEMONITOR_H
#define QEMU_FILEMONITOR_H



typedef struct QFileMonitor QFileMonitor;

typedef enum {
    QFILE_MONITOR_EVENT_CREATED,
    QFILE_MONITOR_EVENT_MODIFIED,
    QFILE_MONITOR_EVENT_DELETED,
    QFILE_MONITOR_EVENT_ATTRIBUTES,
    QFILE_MONITOR_EVENT_IGNORED,
} QFileMonitorEvent;


typedef void (*QFileMonitorHandler)(int64_t id,
                                    QFileMonitorEvent event,
                                    const char *filename,
                                    void *opaque);

QFileMonitor *qemu_file_monitor_new(Error **errp);

void qemu_file_monitor_free(QFileMonitor *mon);

int64_t qemu_file_monitor_add_watch(QFileMonitor *mon,
                                    const char *dirpath,
                                    const char *filename,
                                    QFileMonitorHandler cb,
                                    void *opaque,
                                    Error **errp);

void qemu_file_monitor_remove_watch(QFileMonitor *mon,
                                    const char *dirpath,
                                    int64_t id);

#endif /* QEMU_FILEMONITOR_H */
