#ifndef QEMU_SECCOMP_H
#define QEMU_SECCOMP_H

#define QEMU_SECCOMP_SET_DEFAULT     (1 << 0)
#define QEMU_SECCOMP_SET_OBSOLETE    (1 << 1)
#define QEMU_SECCOMP_SET_PRIVILEGED  (1 << 2)
#define QEMU_SECCOMP_SET_SPAWN       (1 << 3)
#define QEMU_SECCOMP_SET_RESOURCECTL (1 << 4)

int parse_sandbox(void *opaque, QemuOpts *opts, Error **errp);

#endif
