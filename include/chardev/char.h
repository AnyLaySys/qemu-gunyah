#ifndef QEMU_CHAR_H
#define QEMU_CHAR_H

#include "qapi/qapi-types-char.h"
#include "qemu/bitmap.h"
#include "qemu/thread.h"
#include "qom/object.h"

#define IAC_EOR 239
#define IAC_SE 240
#define IAC_NOP 241
#define IAC_BREAK 243
#define IAC_IP 244
#define IAC_SB 250
#define IAC 255

typedef struct CharBackend CharBackend;

typedef enum {
    CHR_EVENT_BREAK, /* serial break char */
    CHR_EVENT_OPENED, /* new connection established */
    CHR_EVENT_MUX_IN, /* mux-focus was set to this terminal */
    CHR_EVENT_MUX_OUT, /* mux-focus will move on */
    CHR_EVENT_CLOSED /* connection closed.  NOTE: currently this event
                      * is only bound to the read port of the chardev.
                      * Normally the read port and write port of a
                      * chardev should be the same, but it can be
                      * different, e.g., for fd chardevs, when the two
                      * fds are different.  So when we received the
                      * CLOSED event it's still possible that the out
                      * port is still open.  TODO: we should only send
                      * the CLOSED event when both ports are closed.
                      */
} QEMUChrEvent;

#define CHR_READ_BUF_LEN 4096

typedef enum {
    QEMU_CHAR_FEATURE_RECONNECTABLE,
    QEMU_CHAR_FEATURE_FD_PASS,
    QEMU_CHAR_FEATURE_GCONTEXT,

    QEMU_CHAR_FEATURE_LAST,
} ChardevFeature;

struct Chardev {
    Object parent_obj;

    QemuMutex chr_write_lock;
    CharBackend *be;
    char *label;
    char *filename;
    int logfd;
    int be_open;
    bool handover_yank_instance;
    GSource *gsource;
    GMainContext *gcontext;
    DECLARE_BITMAP(features, QEMU_CHAR_FEATURE_LAST);
};

Chardev *qemu_chr_new_from_opts(QemuOpts *opts,
                                GMainContext *context,
                                Error **errp);

void qemu_chr_parse_common(QemuOpts *opts, ChardevCommon *backend);

ChardevBackend *qemu_chr_parse_opts(QemuOpts *opts,
                                    Error **errp);

Chardev *qemu_chr_new(const char *label, const char *filename,
                      GMainContext *context);

Chardev *qemu_chr_new_mux_mon(const char *label, const char *filename,
                              GMainContext *context);

void qemu_chr_change(QemuOpts *opts, Error **errp);

void qemu_chr_cleanup(void);

int qemu_chr_be_can_write(Chardev *s);

void qemu_chr_be_write(Chardev *s, const uint8_t *buf, int len);

void qemu_chr_be_write_impl(Chardev *s, const uint8_t *buf, int len);

void qemu_chr_be_update_read_handlers(Chardev *s,
                                      GMainContext *context);

void qemu_chr_be_event(Chardev *s, QEMUChrEvent event);

Chardev *qemu_chr_find(const char *name);

bool qemu_chr_has_feature(Chardev *chr,
                          ChardevFeature feature);
void qemu_chr_set_feature(Chardev *chr,
                          ChardevFeature feature);
QemuOpts *qemu_chr_parse_compat(const char *label, const char *filename,
                                bool permit_mux_mon);
int qemu_chr_write(Chardev *s, const uint8_t *buf, int len, bool write_all);
#define qemu_chr_write_all(s, buf, len) qemu_chr_write(s, buf, len, true)
int qemu_chr_wait_connected(Chardev *chr, Error **errp);

#define TYPE_CHARDEV "chardev"
OBJECT_DECLARE_TYPE(Chardev, ChardevClass, CHARDEV)

#define TYPE_CHARDEV_NULL "chardev-null"
#define TYPE_CHARDEV_MUX "chardev-mux"
#define TYPE_CHARDEV_CONSOLE "chardev-console"
#define TYPE_CHARDEV_STDIO "chardev-stdio"

struct ChardevClass {
    ObjectClass parent_class;

    bool internal; /* TODO: eventually use TYPE_USER_CREATABLE */
    bool supports_yank;

    void (*parse)(QemuOpts *opts, ChardevBackend *backend, Error **errp);

    void (*open)(Chardev *chr, ChardevBackend *backend,
                 bool *be_opened, Error **errp);

    int (*chr_write)(Chardev *s, const uint8_t *buf, int len);

    int (*chr_sync_read)(Chardev *s, const uint8_t *buf, int len);

    GSource *(*chr_add_watch)(Chardev *s, GIOCondition cond);

    void (*chr_update_read_handler)(Chardev *s);

    int (*chr_ioctl)(Chardev *s, int cmd, void *arg);

    int (*get_msgfds)(Chardev *s, int* fds, int num);

    int (*set_msgfds)(Chardev *s, int *fds, int num);

    int (*chr_wait_connected)(Chardev *chr, Error **errp);

    void (*chr_disconnect)(Chardev *chr);

    void (*chr_accept_input)(Chardev *chr);

    void (*chr_set_echo)(Chardev *chr, bool echo);

    void (*chr_set_fe_open)(Chardev *chr, int fe_open);

    void (*chr_be_event)(Chardev *s, QEMUChrEvent event);
};

Chardev *qemu_chardev_new(const char *id, const char *typename,
                          ChardevBackend *backend, GMainContext *context,
                          Error **errp);

extern int term_escape_char;

GSource *qemu_chr_timeout_add_ms(Chardev *chr, guint ms,
                                 GSourceFunc func, void *private);

void suspend_mux_open(void);
void resume_mux_open(void);

#endif
