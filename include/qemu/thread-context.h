
#ifndef SYSEMU_THREAD_CONTEXT_H
#define SYSEMU_THREAD_CONTEXT_H

#include "qapi/qapi-types-machine.h"
#include "qemu/thread.h"
#include "qom/object.h"

#define TYPE_THREAD_CONTEXT "thread-context"
OBJECT_DECLARE_TYPE(ThreadContext, ThreadContextClass,
                    THREAD_CONTEXT)

struct ThreadContextClass {
    ObjectClass parent_class;
};

struct ThreadContext {
    Object parent;

    unsigned int thread_id;
    QemuThread thread;

    QemuSemaphore sem;
    QemuSemaphore sem_thread;
    QemuMutex mutex;

    int thread_cmd;
    void *thread_cmd_data;

    unsigned long *init_cpu_bitmap;
    int init_cpu_nbits;
};

void thread_context_create_thread(ThreadContext *tc, QemuThread *thread,
                                  const char *name,
                                  void *(*start_routine)(void *), void *arg,
                                  int mode);

#endif /* SYSEMU_THREAD_CONTEXT_H */
