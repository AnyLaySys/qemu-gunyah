#ifndef QEMU_HW_VERSION_H
#define QEMU_HW_VERSION_H

#define QEMU_HW_VERSION "2.5+"

void qemu_set_hw_version(const char *);
const char *qemu_hw_version(void);

#endif
