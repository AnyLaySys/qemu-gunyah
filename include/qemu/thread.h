#ifndef QEMU_THREAD_H
#define QEMU_THREAD_H

#include "qemu/processor.h"
#include "qemu/atomic.h"

typedef struct QemuCond QemuCond;
typedef struct QemuSemaphore QemuSemaphore;
typedef struct QemuEvent QemuEvent;
typedef struct QemuLockCnt QemuLockCnt;
typedef struct QemuThread QemuThread;

#ifdef _WIN32
#include "qemu/thread-win32.h"
#else
#include "qemu/thread-posix.h"
#endif

#define QEMU_THREAD_JOINABLE 0
#define QEMU_THREAD_DETACHED 1

void qemu_mutex_init(QemuMutex *mutex);
void qemu_mutex_destroy(QemuMutex *mutex);
int TSA_NO_TSA qemu_mutex_trylock_impl(QemuMutex *mutex, const char *file,
                                       const int line);
void TSA_NO_TSA qemu_mutex_lock_impl(QemuMutex *mutex, const char *file,
                                     const int line);
void TSA_NO_TSA qemu_mutex_unlock_impl(QemuMutex *mutex, const char *file,
                                       const int line);

void qemu_rec_mutex_init(QemuRecMutex *mutex);
void qemu_rec_mutex_destroy(QemuRecMutex *mutex);
void qemu_rec_mutex_lock_impl(QemuRecMutex *mutex, const char *file, int line);
int qemu_rec_mutex_trylock_impl(QemuRecMutex *mutex, const char *file, int line);
void qemu_rec_mutex_unlock_impl(QemuRecMutex *mutex, const char *file, int line);

#define qemu_mutex_lock__raw(m)                         \
        qemu_mutex_lock_impl(m, __FILE__, __LINE__)
#define qemu_mutex_trylock__raw(m)                      \
        qemu_mutex_trylock_impl(m, __FILE__, __LINE__)

#define qemu_mutex_lock(m)                                              \
            qemu_mutex_lock_impl(m, __FILE__, __LINE__)
#define qemu_mutex_trylock(m)                                           \
            qemu_mutex_trylock_impl(m, __FILE__, __LINE__)
#define qemu_rec_mutex_lock(m)                                          \
            qemu_rec_mutex_lock_impl(m, __FILE__, __LINE__)
#define qemu_rec_mutex_trylock(m)                                       \
            qemu_rec_mutex_trylock_impl(m, __FILE__, __LINE__)
#define qemu_cond_wait(c, m)                                            \
            qemu_cond_wait_impl(c, m, __FILE__, __LINE__)
#define qemu_cond_timedwait(c, m, ms)                                   \
            qemu_cond_timedwait_impl(c, m, ms, __FILE__, __LINE__)

#define qemu_mutex_unlock(mutex) \
        qemu_mutex_unlock_impl(mutex, __FILE__, __LINE__)

#define qemu_rec_mutex_unlock(mutex) \
        qemu_rec_mutex_unlock_impl(mutex, __FILE__, __LINE__)

static inline void (qemu_mutex_lock)(QemuMutex *mutex)
{
    qemu_mutex_lock(mutex);
}

static inline int (qemu_mutex_trylock)(QemuMutex *mutex)
{
    return qemu_mutex_trylock(mutex);
}

static inline void (qemu_mutex_unlock)(QemuMutex *mutex)
{
    qemu_mutex_unlock(mutex);
}

static inline void (qemu_rec_mutex_lock)(QemuRecMutex *mutex)
{
    qemu_rec_mutex_lock(mutex);
}

static inline int (qemu_rec_mutex_trylock)(QemuRecMutex *mutex)
{
    return qemu_rec_mutex_trylock(mutex);
}

static inline void (qemu_rec_mutex_unlock)(QemuRecMutex *mutex)
{
    qemu_rec_mutex_unlock(mutex);
}

void qemu_cond_init(QemuCond *cond);
void qemu_cond_destroy(QemuCond *cond);

void qemu_cond_signal(QemuCond *cond);
void qemu_cond_broadcast(QemuCond *cond);
void TSA_NO_TSA qemu_cond_wait_impl(QemuCond *cond, QemuMutex *mutex,
                                    const char *file, const int line);
bool qemu_cond_timedwait_impl(QemuCond *cond, QemuMutex *mutex, int ms,
                              const char *file, const int line);

static inline void (qemu_cond_wait)(QemuCond *cond, QemuMutex *mutex)
{
    qemu_cond_wait(cond, mutex);
}

static inline bool (qemu_cond_timedwait)(QemuCond *cond, QemuMutex *mutex,
                                         int ms)
{
    return qemu_cond_timedwait(cond, mutex, ms);
}

void qemu_sem_init(QemuSemaphore *sem, int init);
void qemu_sem_post(QemuSemaphore *sem);
void qemu_sem_wait(QemuSemaphore *sem);
int qemu_sem_timedwait(QemuSemaphore *sem, int ms);
void qemu_sem_destroy(QemuSemaphore *sem);

void qemu_event_init(QemuEvent *ev, bool init);
void qemu_event_set(QemuEvent *ev);
void qemu_event_reset(QemuEvent *ev);
void qemu_event_wait(QemuEvent *ev);
void qemu_event_destroy(QemuEvent *ev);

void qemu_thread_create(QemuThread *thread, const char *name,
                        void *(*start_routine)(void *),
                        void *arg, int mode);
int qemu_thread_set_affinity(QemuThread *thread, unsigned long *host_cpus,
                             unsigned long nbits);
int qemu_thread_get_affinity(QemuThread *thread, unsigned long **host_cpus,
                             unsigned long *nbits);
void *qemu_thread_join(QemuThread *thread);
void qemu_thread_get_self(QemuThread *thread);
bool qemu_thread_is_self(QemuThread *thread);
G_NORETURN void qemu_thread_exit(void *retval);
void qemu_thread_naming(bool enable);

struct Notifier;
void qemu_thread_atexit_add(struct Notifier *notifier);
void qemu_thread_atexit_remove(struct Notifier *notifier);

void qemu_thread_init_tls(void);


#ifdef CONFIG_TSAN
#include <sanitizer/tsan_interface.h>
#endif

struct QemuSpin {
    int value;
};

static inline void qemu_spin_init(QemuSpin *spin)
{
    qatomic_set(&spin->value, 0);
#ifdef CONFIG_TSAN
    __tsan_mutex_create(spin, __tsan_mutex_not_static);
#endif
}

static inline void qemu_spin_destroy(QemuSpin *spin)
{
#ifdef CONFIG_TSAN
    __tsan_mutex_destroy(spin, __tsan_mutex_not_static);
#endif
}

static inline void qemu_spin_lock(QemuSpin *spin)
{
#ifdef CONFIG_TSAN
    __tsan_mutex_pre_lock(spin, 0);
#endif
    while (unlikely(qatomic_xchg(&spin->value, 1))) {
        while (qatomic_read(&spin->value)) {
            cpu_relax();
        }
    }
#ifdef CONFIG_TSAN
    __tsan_mutex_post_lock(spin, 0, 0);
#endif
}

static inline bool qemu_spin_trylock(QemuSpin *spin)
{
#ifdef CONFIG_TSAN
    __tsan_mutex_pre_lock(spin, __tsan_mutex_try_lock);
#endif
    bool busy = qatomic_xchg(&spin->value, true);
#ifdef CONFIG_TSAN
    unsigned flags = __tsan_mutex_try_lock;
    flags |= busy ? __tsan_mutex_try_lock_failed : 0;
    __tsan_mutex_post_lock(spin, flags, 0);
#endif
    return busy;
}

static inline bool qemu_spin_locked(QemuSpin *spin)
{
    return qatomic_read(&spin->value);
}

static inline void qemu_spin_unlock(QemuSpin *spin)
{
#ifdef CONFIG_TSAN
    __tsan_mutex_pre_unlock(spin, 0);
#endif
    qatomic_store_release(&spin->value, 0);
#ifdef CONFIG_TSAN
    __tsan_mutex_post_unlock(spin, 0);
#endif
}

#endif
