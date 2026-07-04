
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "block/graph-lock.h"
#include "qapi/qapi-builtin-types.h"

#define SNAPSHOT_OPT_BASE       "snapshot."
#define SNAPSHOT_OPT_ID         "snapshot.id"
#define SNAPSHOT_OPT_NAME       "snapshot.name"

extern QemuOptsList internal_snapshot_opts;

typedef struct QEMUSnapshotInfo {
    char id_str[128]; /* unique snapshot id */
    char name[256]; /* user chosen name */
    uint64_t vm_state_size; /* VM state info size */
    uint32_t date_sec; /* UTC date of the snapshot */
    uint32_t date_nsec;
    uint64_t vm_clock_nsec; /* VM clock relative to boot */
    uint64_t icount; /* record/replay step */
} QEMUSnapshotInfo;


int bdrv_snapshot_find(BlockDriverState *bs, QEMUSnapshotInfo *sn_info,
                       const char *name);
bool bdrv_snapshot_find_by_id_and_name(BlockDriverState *bs,
                                       const char *id,
                                       const char *name,
                                       QEMUSnapshotInfo *sn_info,
                                       Error **errp);

int GRAPH_RDLOCK bdrv_can_snapshot(BlockDriverState *bs);

int GRAPH_RDLOCK
bdrv_snapshot_create(BlockDriverState *bs, QEMUSnapshotInfo *sn_info);

int GRAPH_UNLOCKED
bdrv_snapshot_goto(BlockDriverState *bs, const char *snapshot_id, Error **errp);

int GRAPH_RDLOCK
bdrv_snapshot_delete(BlockDriverState *bs, const char *snapshot_id,
                     const char *name, Error **errp);

int bdrv_snapshot_list(BlockDriverState *bs,
                       QEMUSnapshotInfo **psn_info);
int bdrv_snapshot_load_tmp(BlockDriverState *bs,
                           const char *snapshot_id,
                           const char *name,
                           Error **errp);
int bdrv_snapshot_load_tmp_by_id_or_name(BlockDriverState *bs,
                                         const char *id_or_name,
                                         Error **errp);



bool bdrv_all_can_snapshot(bool has_devices, strList *devices,
                           Error **errp);
int bdrv_all_delete_snapshot(const char *name,
                             bool has_devices, strList *devices,
                             Error **errp);
int bdrv_all_goto_snapshot(const char *name,
                           bool has_devices, strList *devices,
                           Error **errp);
int bdrv_all_has_snapshot(const char *name,
                          bool has_devices, strList *devices,
                          Error **errp);
int bdrv_all_create_snapshot(QEMUSnapshotInfo *sn,
                             BlockDriverState *vm_state_bs,
                             uint64_t vm_state_size,
                             bool has_devices,
                             strList *devices,
                             Error **errp);

BlockDriverState *bdrv_all_find_vmstate_bs(const char *vmstate_bs,
                                           bool has_devices, strList *devices,
                                           Error **errp);

#endif
