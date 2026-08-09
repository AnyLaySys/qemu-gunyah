
#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include "block/block.h"
#include "qemu/queue.h"

typedef enum {
    IF_DEFAULT = -1,            /* for use with drive_add() only */
    IF_NONE = 0,
    IF_IDE, IF_SCSI, IF_VIRTIO, IF_XEN,
    IF_COUNT
} BlockInterfaceType;

struct DriveInfo {
    BlockInterfaceType type;
    int bus;
    int unit;
    int auto_del;               /* see blockdev_mark_auto_del() */
    bool is_default;            /* Added by default_drive() ?  */
    int media_cd;
    QemuOpts *opts;
    QTAILQ_ENTRY(DriveInfo) next;
};


void blockdev_mark_auto_del(BlockBackend *blk);
void blockdev_auto_del(BlockBackend *blk);

DriveInfo *blk_legacy_dinfo(BlockBackend *blk);
DriveInfo *blk_set_legacy_dinfo(BlockBackend *blk, DriveInfo *dinfo);
BlockBackend *blk_by_legacy_dinfo(DriveInfo *dinfo);

void override_max_devs(BlockInterfaceType type, int max_devs);

DriveInfo *drive_get(BlockInterfaceType type, int bus, int unit);
void drive_check_orphaned(void);
DriveInfo *drive_get_by_index(BlockInterfaceType type, int index);
int drive_get_max_bus(BlockInterfaceType type);

QemuOpts *drive_add(BlockInterfaceType type, int index, const char *file,
                    const char *optstr);
DriveInfo *drive_new(QemuOpts *arg, BlockInterfaceType block_default_type,
                     Error **errp);

#endif
