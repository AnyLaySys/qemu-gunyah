#ifndef QEMU_MMAP_ALLOC_H
#define QEMU_MMAP_ALLOC_H

typedef enum {
    QEMU_FS_TYPE_UNKNOWN = 0,
    QEMU_FS_TYPE_TMPFS,
    QEMU_FS_TYPE_HUGETLBFS,
    QEMU_FS_TYPE_NUM,
} QemuFsType;

size_t qemu_fd_getpagesize(int fd);
QemuFsType qemu_fd_getfs(int fd);

void *qemu_ram_mmap(int fd,
                    size_t size,
                    size_t align,
                    uint32_t qemu_map_flags,
                    off_t map_offset);

void qemu_ram_munmap(int fd, void *ptr, size_t size);


#define QEMU_MAP_READONLY   (1 << 0)

#define QEMU_MAP_SHARED     (1 << 1)

#define QEMU_MAP_SYNC       (1 << 2)

#define QEMU_MAP_NORESERVE  (1 << 3)

#endif
