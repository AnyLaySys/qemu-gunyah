
#ifndef QEMU_LOCKABLE_H
#define QEMU_LOCKABLE_H

#include "qemu/coroutine-core.h"
#include "qemu/thread.h"

typedef void QemuLockUnlockFunc(void *);

typedef struct QemuLockable {
    void *object;
    QemuLockUnlockFunc *lock;
    QemuLockUnlockFunc *unlock;
} QemuLockable;

static inline __attribute__((__always_inline__)) QemuLockable *
qemu_make_lockable(void *x, QemuLockable *lockable)
{
    return x ? lockable : NULL;
}

static inline __attribute__((__always_inline__)) QemuLockable *
qemu_null_lockable(void *x)
{
    if (x != NULL) {
        qemu_build_not_reached();
    }
    return NULL;
}

#define QML_FUNC_(name)                                           \
    static inline void qemu_lockable_ ## name ## _lock(void *x)   \
    {                                                             \
        qemu_ ## name ## _lock(x);                                \
    }                                                             \
    static inline void qemu_lockable_ ## name ## _unlock(void *x) \
    {                                                             \
        qemu_ ## name ## _unlock(x);                              \
    }

QML_FUNC_(mutex)
QML_FUNC_(rec_mutex)
QML_FUNC_(co_mutex)
QML_FUNC_(spin)

#define QML_OBJ_(x, name) (&(QemuLockable) {        \
        .object = (x),                              \
        .lock = qemu_lockable_ ## name ## _lock,    \
        .unlock = qemu_lockable_ ## name ## _unlock \
    })

#define QEMU_MAKE_LOCKABLE(x)                                           \
    _Generic((x), QemuLockable *: (x),                                  \
             void *: qemu_null_lockable(x),                             \
             QemuMutex *: qemu_make_lockable(x, QML_OBJ_(x, mutex)),    \
             QemuRecMutex *: qemu_make_lockable(x, QML_OBJ_(x, rec_mutex)), \
             CoMutex *: qemu_make_lockable(x, QML_OBJ_(x, co_mutex)),   \
             QemuSpin *: qemu_make_lockable(x, QML_OBJ_(x, spin)))

#define QEMU_MAKE_LOCKABLE_NONNULL(x)                           \
    _Generic((x), QemuLockable *: (x),                          \
                  QemuMutex *: QML_OBJ_(x, mutex),              \
                  QemuRecMutex *: QML_OBJ_(x, rec_mutex),       \
                  CoMutex *: QML_OBJ_(x, co_mutex),             \
                  QemuSpin *: QML_OBJ_(x, spin))

static inline void qemu_lockable_lock(QemuLockable *x)
{
    x->lock(x->object);
}

static inline void qemu_lockable_unlock(QemuLockable *x)
{
    x->unlock(x->object);
}

static inline QemuLockable *qemu_lockable_auto_lock(QemuLockable *x)
{
    qemu_lockable_lock(x);
    return x;
}

static inline void qemu_lockable_auto_unlock(QemuLockable *x)
{
    if (x) {
        qemu_lockable_unlock(x);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QemuLockable, qemu_lockable_auto_unlock)

#define WITH_QEMU_LOCK_GUARD_(x, var) \
    for (g_autoptr(QemuLockable) var = \
                qemu_lockable_auto_lock(QEMU_MAKE_LOCKABLE_NONNULL((x))); \
         var; \
         qemu_lockable_auto_unlock(var), var = NULL)

#define WITH_QEMU_LOCK_GUARD(x) \
    WITH_QEMU_LOCK_GUARD_((x), glue(qemu_lockable_auto, __COUNTER__))

#define QEMU_LOCK_GUARD(x)                                       \
    g_autoptr(QemuLockable)                                      \
    glue(qemu_lockable_auto, __COUNTER__) G_GNUC_UNUSED =        \
            qemu_lockable_auto_lock(QEMU_MAKE_LOCKABLE((x)))

#endif
