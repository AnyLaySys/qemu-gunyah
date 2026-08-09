
#ifndef JOB_H
#define JOB_H

#include "qapi/qapi-types-job.h"
#include "qemu/queue.h"
#include "qemu/progress_meter.h"
#include "qemu/coroutine.h"
#include "block/aio.h"

typedef struct JobDriver JobDriver;
typedef struct JobTxn JobTxn;


typedef struct Job {


    char *id;

    const JobDriver *driver;

    Coroutine *co;

    bool auto_finalize;

    bool auto_dismiss;

    BlockCompletionFunc *cb;

    void *opaque;

    ProgressMeter progress;

    AioContext *aio_context;



    int refcnt;

    JobStatus status;

    QEMUTimer sleep_timer;

    int pause_count;

    bool busy;

    bool paused;

    bool user_paused;

    bool cancelled;

    bool force_cancel;

    bool deferred_to_main_loop;

    int ret;

    Error *err;

    NotifierList on_finalize_cancelled;

    NotifierList on_finalize_completed;

    NotifierList on_pending;

    NotifierList on_ready;

    NotifierList on_idle;

    QLIST_ENTRY(Job) job_list;

    JobTxn *txn;

    QLIST_ENTRY(Job) txn_list;
} Job;

struct JobDriver {


    size_t instance_size;

    JobType job_type;

    int coroutine_fn (*run)(Job *job, Error **errp);


    void coroutine_fn (*pause)(Job *job);

    void coroutine_fn (*resume)(Job *job);


    void (*user_resume)(Job *job);

    void (*complete)(Job *job, Error **errp);

    int (*prepare)(Job *job);

    void (*commit)(Job *job);

    void (*abort)(Job *job);

    void (*clean)(Job *job);

    bool (*cancel)(Job *job, bool force);


    void (*free)(Job *job);
};

typedef enum JobCreateFlags {
    JOB_DEFAULT = 0x00,
    JOB_INTERNAL = 0x01,
    JOB_MANUAL_FINALIZE = 0x02,
    JOB_MANUAL_DISMISS = 0x04,
} JobCreateFlags;

extern QemuMutex job_mutex;

#define JOB_LOCK_GUARD() QEMU_LOCK_GUARD(&job_mutex)

#define WITH_JOB_LOCK_GUARD() WITH_QEMU_LOCK_GUARD(&job_mutex)

void job_lock(void);

void job_unlock(void);

JobTxn *job_txn_new(void);

void job_txn_unref(JobTxn *txn);

void job_txn_unref_locked(JobTxn *txn);

void *job_create(const char *job_id, const JobDriver *driver, JobTxn *txn,
                 AioContext *ctx, int flags, BlockCompletionFunc *cb,
                 void *opaque, Error **errp);

void job_ref_locked(Job *job);

void job_unref_locked(Job *job);

void job_progress_update(Job *job, uint64_t done);

void job_progress_set_remaining(Job *job, uint64_t remaining);

void job_progress_increase_remaining(Job *job, uint64_t delta);

void job_enter_cond_locked(Job *job, bool(*fn)(Job *job));

void job_start(Job *job);

void job_enter(Job *job);

void coroutine_fn GRAPH_UNLOCKED job_pause_point(Job *job);

void coroutine_fn job_yield(Job *job);

void coroutine_fn job_sleep_ns(Job *job, int64_t ns);

JobType job_type(const Job *job);

const char *job_type_str(const Job *job);

bool job_is_internal(Job *job);

bool job_is_cancelled(Job *job);

bool job_is_cancelled_locked(Job *job);

bool job_cancel_requested(Job *job);

bool job_is_completed_locked(Job *job);

bool job_is_ready(Job *job);

bool job_is_ready_locked(Job *job);

bool job_is_paused(Job *job);

void job_pause(Job *job);

void job_pause_locked(Job *job);

void job_resume(Job *job);

void job_resume_locked(Job *job);

void job_user_pause_locked(Job *job, Error **errp);

bool job_user_paused_locked(Job *job);

void job_user_resume_locked(Job *job, Error **errp);

Job *job_next(Job *job);

Job *job_next_locked(Job *job);

Job *job_get_locked(const char *id);

int job_apply_verb_locked(Job *job, JobVerb verb, Error **errp);

void job_early_fail(Job *job);

void job_transition_to_ready(Job *job);

void job_complete_locked(Job *job, Error **errp);

void job_cancel_locked(Job *job, bool force);

void job_user_cancel_locked(Job *job, bool force, Error **errp);

int job_cancel_sync(Job *job, bool force);

int job_cancel_sync_locked(Job *job, bool force);

int job_complete_sync_locked(Job *job, Error **errp);

void job_finalize_locked(Job *job, Error **errp);

void job_dismiss_locked(Job **job, Error **errp);

int job_finish_sync_locked(Job *job, void (*finish)(Job *, Error **errp),
                           Error **errp);

void job_set_aio_context(Job *job, AioContext *ctx);

#endif
