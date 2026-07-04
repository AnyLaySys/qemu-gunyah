
#ifndef QEMU_MAIN_LOOP_H
#define QEMU_MAIN_LOOP_H

#include "block/aio.h"
#include "qom/object.h"
#include "system/event-loop-base.h"

#define SIG_IPI SIGUSR1

#define TYPE_MAIN_LOOP  "main-loop"
OBJECT_DECLARE_TYPE(MainLoop, MainLoopClass, MAIN_LOOP)

struct MainLoop {
    EventLoopBase parent_obj;
};
typedef struct MainLoop MainLoop;

int qemu_init_main_loop(Error **errp);

void main_loop_wait(int nonblocking);

AioContext *qemu_get_aio_context(void);

void qemu_notify_event(void);

#ifdef _WIN32
typedef int PollingFunc(void *opaque);

int qemu_add_polling_cb(PollingFunc *func, void *opaque);

void qemu_del_polling_cb(PollingFunc *func, void *opaque);

typedef void WaitObjectFunc(void *opaque);

int qemu_add_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque);

void qemu_del_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque);
#endif


typedef void IOReadHandler(void *opaque, const uint8_t *buf, int size);

typedef int IOCanReadHandler(void *opaque);

void qemu_set_fd_handler(int fd,
                         IOHandler *fd_read,
                         IOHandler *fd_write,
                         void *opaque);


void event_notifier_set_handler(EventNotifier *e,
                                EventNotifierHandler *handler);

GSource *iohandler_get_g_source(void);
AioContext *iohandler_get_aio_context(void);

void rust_bql_mock_lock(void);

bool bql_locked(void);

void bql_block_unlock(bool increase);

bool qemu_in_main_thread(void);

#define GLOBAL_STATE_CODE()                                         \
    do {                                                            \
        assert(qemu_in_main_thread());                              \
    } while (0)

#define IO_CODE()                                                   \
    do {                                                            \
        /* nop */                                                   \
    } while (0)

#define IO_OR_GS_CODE()                                             \
    do {                                                            \
        /* nop */                                                   \
    } while (0)

#define bql_lock() bql_lock_impl(__FILE__, __LINE__)
void bql_lock_impl(const char *file, int line);

void bql_unlock(void);

typedef struct BQLLockAuto BQLLockAuto;

static inline BQLLockAuto *bql_auto_lock(const char *file, int line)
{
    if (bql_locked()) {
        return NULL;
    }
    bql_lock_impl(file, line);
    return (BQLLockAuto *)(uintptr_t)1;
}

static inline void bql_auto_unlock(BQLLockAuto *l)
{
    bql_unlock();
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(BQLLockAuto, bql_auto_unlock)

#define BQL_LOCK_GUARD() \
    g_autoptr(BQLLockAuto) _bql_lock_auto __attribute__((unused)) \
        = bql_auto_lock(__FILE__, __LINE__)

void qemu_cond_wait_bql(QemuCond *cond);

void qemu_cond_timedwait_bql(QemuCond *cond, int ms);


#define qemu_bh_new_guarded(cb, opaque, guard) \
    qemu_bh_new_full((cb), (opaque), (stringify(cb)), guard)
#define qemu_bh_new(cb, opaque) \
    qemu_bh_new_full((cb), (opaque), (stringify(cb)), NULL)
QEMUBH *qemu_bh_new_full(QEMUBHFunc *cb, void *opaque, const char *name,
                         MemReentrancyGuard *reentrancy_guard);
void qemu_bh_schedule_idle(QEMUBH *bh);

enum {
    MAIN_LOOP_POLL_FILL,
    MAIN_LOOP_POLL_ERR,
    MAIN_LOOP_POLL_OK,
};

typedef struct MainLoopPoll {
    int state;
    uint32_t timeout;
    GArray *pollfds;
} MainLoopPoll;

void main_loop_poll_add_notifier(Notifier *notify);
void main_loop_poll_remove_notifier(Notifier *notify);

#endif
