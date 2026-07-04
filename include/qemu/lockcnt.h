
#ifndef QEMU_LOCKCNT_H
#define QEMU_LOCKCNT_H

#include "qemu/thread.h"

typedef struct QemuLockCnt QemuLockCnt;

struct QemuLockCnt {
#ifndef CONFIG_LINUX
    QemuMutex mutex;
#endif
    unsigned count;
};

void qemu_lockcnt_init(QemuLockCnt *lockcnt);

void qemu_lockcnt_destroy(QemuLockCnt *lockcnt);

void qemu_lockcnt_inc(QemuLockCnt *lockcnt);

void qemu_lockcnt_dec(QemuLockCnt *lockcnt);

bool qemu_lockcnt_dec_and_lock(QemuLockCnt *lockcnt);

bool qemu_lockcnt_dec_if_lock(QemuLockCnt *lockcnt);

void qemu_lockcnt_lock(QemuLockCnt *lockcnt);

void qemu_lockcnt_unlock(QemuLockCnt *lockcnt);

void qemu_lockcnt_inc_and_unlock(QemuLockCnt *lockcnt);

unsigned qemu_lockcnt_count(QemuLockCnt *lockcnt);

#endif
