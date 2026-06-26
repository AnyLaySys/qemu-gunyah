
#ifndef QAUTHZ_LISTFILE_H
#define QAUTHZ_LISTFILE_H

#include "authz/list.h"
#include "qemu/filemonitor.h"
#include "qom/object.h"

#define TYPE_QAUTHZ_LIST_FILE "authz-list-file"

OBJECT_DECLARE_SIMPLE_TYPE(QAuthZListFile,
                           QAUTHZ_LIST_FILE)



struct QAuthZListFile {
    QAuthZ parent_obj;

    QAuthZ *list;
    char *filename;
    bool refresh;
    QFileMonitor *file_monitor;
    int64_t file_watch;
};




QAuthZListFile *qauthz_list_file_new(const char *id,
                                     const char *filename,
                                     bool refresh,
                                     Error **errp);

#endif /* QAUTHZ_LISTFILE_H */
