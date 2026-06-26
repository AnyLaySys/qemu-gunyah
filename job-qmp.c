
#include "qemu/osdep.h"
#include "qemu/job.h"
#include "qapi/qapi-commands-job.h"
#include "qapi/error.h"
#include "trace/trace-root.h"

static Job *find_job_locked(const char *id, Error **errp)
{
    Job *job;

    job = job_get_locked(id);
    if (!job) {
        error_setg(errp, "Job not found");
        return NULL;
    }

    return job;
}

void qmp_job_cancel(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_cancel(job);
    job_user_cancel_locked(job, true, errp);
}

void qmp_job_pause(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_pause(job);
    job_user_pause_locked(job, errp);
}

void qmp_job_resume(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_resume(job);
    job_user_resume_locked(job, errp);
}

void qmp_job_complete(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_complete(job);
    job_complete_locked(job, errp);
}

void qmp_job_finalize(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_finalize(job);
    job_ref_locked(job);
    job_finalize_locked(job, errp);

    job_unref_locked(job);
}

void qmp_job_dismiss(const char *id, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();
    job = find_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_job_dismiss(job);
    job_dismiss_locked(&job, errp);
}

static JobInfo *job_query_single_locked(Job *job, Error **errp)
{
    JobInfo *info;
    uint64_t progress_current;
    uint64_t progress_total;

    assert(!job_is_internal(job));
    progress_get_snapshot(&job->progress, &progress_current,
                          &progress_total);

    info = g_new(JobInfo, 1);
    *info = (JobInfo) {
        .id                 = g_strdup(job->id),
        .type               = job_type(job),
        .status             = job->status,
        .current_progress   = progress_current,
        .total_progress     = progress_total,
        .error              = job->err ?
                              g_strdup(error_get_pretty(job->err)) : NULL,
    };

    return info;
}

JobInfoList *qmp_query_jobs(Error **errp)
{
    JobInfoList *head = NULL, **tail = &head;
    Job *job;

    JOB_LOCK_GUARD();

    for (job = job_next_locked(NULL); job; job = job_next_locked(job)) {
        JobInfo *value;

        if (job_is_internal(job)) {
            continue;
        }
        value = job_query_single_locked(job, errp);
        if (!value) {
            qapi_free_JobInfoList(head);
            return NULL;
        }
        QAPI_LIST_APPEND(tail, value);
    }

    return head;
}
