
#ifndef IOTHREAD_H
#define IOTHREAD_H

#include "block/aio.h"
#include "qemu/thread.h"
#include "qom/object.h"
#include "system/event-loop-base.h"

#define TYPE_IOTHREAD "iothread"

struct IOThread {
    EventLoopBase parent_obj;

    QemuThread thread;
    AioContext *ctx;
    bool run_gcontext;          /* whether we should run gcontext */
    GMainContext *worker_context;
    GMainLoop *main_loop;
    QemuSemaphore init_done_sem; /* is thread init done? */
    bool stopping;              /* has iothread_stop() been called? */
    bool running;               /* should iothread_run() continue? */
    int thread_id;

    int64_t poll_max_ns;
    int64_t poll_grow;
    int64_t poll_shrink;
};
typedef struct IOThread IOThread;

DECLARE_INSTANCE_CHECKER(IOThread, IOTHREAD,
                         TYPE_IOTHREAD)

char *iothread_get_id(IOThread *iothread);
IOThread *iothread_by_id(const char *id);
AioContext *iothread_get_aio_context(IOThread *iothread);
GMainContext *iothread_get_g_main_context(IOThread *iothread);

IOThread *iothread_create(const char *id, Error **errp);
void iothread_stop(IOThread *iothread);
void iothread_destroy(IOThread *iothread);

bool qemu_in_iothread(void);

#endif /* IOTHREAD_H */
