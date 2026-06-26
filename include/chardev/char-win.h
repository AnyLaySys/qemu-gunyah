#ifndef CHAR_WIN_H
#define CHAR_WIN_H

#include "chardev/char.h"
#include "qom/object.h"

struct WinChardev {
    Chardev parent;

    bool keep_open; /* console do not close file */
    HANDLE file, hrecv, hsend;
    OVERLAPPED orecv;
    BOOL fpipe;

    OVERLAPPED osend;
};
typedef struct WinChardev WinChardev;

#define NSENDBUF 2048
#define NRECVBUF 2048

#define TYPE_CHARDEV_WIN "chardev-win"
DECLARE_INSTANCE_CHECKER(WinChardev, WIN_CHARDEV,
                         TYPE_CHARDEV_WIN)

void win_chr_set_file(Chardev *chr, HANDLE file, bool keep_open);
int win_chr_serial_init(Chardev *chr, const char *filename, Error **errp);
int win_chr_pipe_poll(void *opaque);

#endif /* CHAR_WIN_H */
