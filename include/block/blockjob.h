
#ifndef BLOCKJOB_H
#define BLOCKJOB_H

#include "qapi/qapi-types-block-core.h"
#include "qemu/job.h"
#include "qemu/ratelimit.h"

#define BLOCK_JOB_SLICE_TIME 100000000ULL /* ns */

typedef struct BlockJobDriver BlockJobDriver;

typedef struct BlockJob {
    Job job;

    BlockDeviceIoStatus iostatus;

    int64_t speed;

    RateLimit limit;

    Error *blocker;


    Notifier finalize_cancelled_notifier;

    Notifier finalize_completed_notifier;

    Notifier pending_notifier;

    Notifier ready_notifier;

    Notifier idle_notifier;

    GSList *nodes;
} BlockJob;


BlockJob *block_job_next_locked(BlockJob *job);

BlockJob *block_job_get(const char *id);

BlockJob *block_job_get_locked(const char *id);

int GRAPH_WRLOCK
block_job_add_bdrv(BlockJob *job, const char *name, BlockDriverState *bs,
                   uint64_t perm, uint64_t shared_perm, Error **errp);

void block_job_remove_all_bdrv(BlockJob *job);

bool block_job_has_bdrv(BlockJob *job, BlockDriverState *bs);

bool block_job_set_speed_locked(BlockJob *job, int64_t speed, Error **errp);

void block_job_change_locked(BlockJob *job, BlockJobChangeOptions *opts,
                             Error **errp);

BlockJobInfo *block_job_query_locked(BlockJob *job, Error **errp);

void block_job_iostatus_reset_locked(BlockJob *job);

AioContext *block_job_get_aio_context(BlockJob *job);



bool block_job_is_internal(BlockJob *job);

const BlockJobDriver *block_job_driver(BlockJob *job);

#endif
