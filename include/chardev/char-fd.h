#ifndef CHAR_FD_H
#define CHAR_FD_H

#include "io/channel.h"
#include "chardev/char.h"
#include "qom/object.h"

struct FDChardev {
    Chardev parent;

    QIOChannel *ioc_in, *ioc_out;
    int max_size;
};
typedef struct FDChardev FDChardev;

#define TYPE_CHARDEV_FD "chardev-fd"

DECLARE_INSTANCE_CHECKER(FDChardev, FD_CHARDEV,
                         TYPE_CHARDEV_FD)

void qemu_chr_open_fd(Chardev *chr, int fd_in, int fd_out);

#endif /* CHAR_FD_H */
