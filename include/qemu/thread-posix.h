#ifndef QEMU_THREAD_POSIX_H
#define QEMU_THREAD_POSIX_H

#include <pthread.h>
#include <semaphore.h>

struct QemuMutex {
    pthread_mutex_t lock;
#ifdef CONFIG_DEBUG_MUTEX
    const char *file;
    int line;
#endif
    bool initialized;
};

typedef struct QemuRecMutex {
    QemuMutex m;
} QemuRecMutex;

struct QemuCond {
    pthread_cond_t cond;
    bool initialized;
};

struct QemuSemaphore {
    QemuMutex mutex;
    QemuCond cond;
    unsigned int count;
};

struct QemuEvent {
#ifndef __linux__
    pthread_mutex_t lock;
    pthread_cond_t cond;
#endif
    unsigned value;
    bool initialized;
};

struct QemuThread {
    pthread_t thread;
};

#endif
