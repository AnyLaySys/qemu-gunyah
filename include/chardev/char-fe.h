#ifndef QEMU_CHAR_FE_H
#define QEMU_CHAR_FE_H

#include "chardev/char.h"
#include "qemu/main-loop.h"

typedef void IOEventHandler(void *opaque, QEMUChrEvent event);
typedef int BackendChangeHandler(void *opaque);

struct CharBackend {
    Chardev *chr;
    IOEventHandler *chr_event;
    IOCanReadHandler *chr_can_read;
    IOReadHandler *chr_read;
    BackendChangeHandler *chr_be_change;
    void *opaque;
    unsigned int tag;
    bool fe_is_open;
};

bool qemu_chr_fe_init(CharBackend *b, Chardev *s, Error **errp);

void qemu_chr_fe_deinit(CharBackend *b, bool del);

Chardev *qemu_chr_fe_get_driver(CharBackend *be);

bool qemu_chr_fe_backend_connected(CharBackend *be);

bool qemu_chr_fe_backend_open(CharBackend *be);

void qemu_chr_fe_set_handlers_full(CharBackend *b,
                                   IOCanReadHandler *fd_can_read,
                                   IOReadHandler *fd_read,
                                   IOEventHandler *fd_event,
                                   BackendChangeHandler *be_change,
                                   void *opaque,
                                   GMainContext *context,
                                   bool set_open,
                                   bool sync_state);

void qemu_chr_fe_set_handlers(CharBackend *b,
                              IOCanReadHandler *fd_can_read,
                              IOReadHandler *fd_read,
                              IOEventHandler *fd_event,
                              BackendChangeHandler *be_change,
                              void *opaque,
                              GMainContext *context,
                              bool set_open);

void qemu_chr_fe_take_focus(CharBackend *b);

void qemu_chr_fe_accept_input(CharBackend *be);

void qemu_chr_fe_disconnect(CharBackend *be);

int qemu_chr_fe_wait_connected(CharBackend *be, Error **errp);

void qemu_chr_fe_set_echo(CharBackend *be, bool echo);

void qemu_chr_fe_set_open(CharBackend *be, bool is_open);

void qemu_chr_fe_printf(CharBackend *be, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);


typedef gboolean (*FEWatchFunc)(void *do_not_use, GIOCondition condition, void *data);

guint qemu_chr_fe_add_watch(CharBackend *be, GIOCondition cond,
                            FEWatchFunc func, void *user_data);

int qemu_chr_fe_write(CharBackend *be, const uint8_t *buf, int len);

int qemu_chr_fe_write_all(CharBackend *be, const uint8_t *buf, int len);

int qemu_chr_fe_read_all(CharBackend *be, uint8_t *buf, int len);

int qemu_chr_fe_ioctl(CharBackend *be, int cmd, void *arg);

int qemu_chr_fe_get_msgfd(CharBackend *be);

int qemu_chr_fe_get_msgfds(CharBackend *be, int *fds, int num);

int qemu_chr_fe_set_msgfds(CharBackend *be, int *fds, int num);

#endif /* QEMU_CHAR_FE_H */
