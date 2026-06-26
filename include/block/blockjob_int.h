
#ifndef BLOCKJOB_INT_H
#define BLOCKJOB_INT_H

#include "block/blockjob.h"

struct BlockJobDriver {
    JobDriver job_driver;


    bool (*drained_poll)(BlockJob *job);


    void (*attached_aio_context)(BlockJob *job, AioContext *new_context);

    void (*set_speed)(BlockJob *job, int64_t speed);

    void (*change)(BlockJob *job, BlockJobChangeOptions *opts, Error **errp);

    void (*query)(BlockJob *job, BlockJobInfo *info);
};


void * GRAPH_UNLOCKED
block_job_create(const char *job_id, const BlockJobDriver *driver,
                 JobTxn *txn, BlockDriverState *bs, uint64_t perm,
                 uint64_t shared_perm, int64_t speed, int flags,
                 BlockCompletionFunc *cb, void *opaque, Error **errp);

void block_job_free(Job *job);

void block_job_user_resume(Job *job);


void block_job_ratelimit_processed_bytes(BlockJob *job, uint64_t n);

void block_job_ratelimit_sleep(BlockJob *job);

BlockErrorAction block_job_error_action(BlockJob *job, BlockdevOnError on_err,
                                        int is_read, int error);

#endif
