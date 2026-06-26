#ifndef QEMU_CLIPBOARD_H
#define QEMU_CLIPBOARD_H

#include "qemu/notify.h"


typedef enum QemuClipboardType QemuClipboardType;
typedef enum QemuClipboardNotifyType QemuClipboardNotifyType;
typedef enum QemuClipboardSelection QemuClipboardSelection;
typedef struct QemuClipboardPeer QemuClipboardPeer;
typedef struct QemuClipboardNotify QemuClipboardNotify;
typedef struct QemuClipboardInfo QemuClipboardInfo;

enum QemuClipboardType {
    QEMU_CLIPBOARD_TYPE_TEXT,
    QEMU_CLIPBOARD_TYPE__COUNT,
};

enum QemuClipboardSelection {
    QEMU_CLIPBOARD_SELECTION_CLIPBOARD,
    QEMU_CLIPBOARD_SELECTION_PRIMARY,
    QEMU_CLIPBOARD_SELECTION_SECONDARY,
    QEMU_CLIPBOARD_SELECTION__COUNT,
};

struct QemuClipboardPeer {
    const char *name;
    Notifier notifier;
    void (*request)(QemuClipboardInfo *info,
                    QemuClipboardType type);
};

enum QemuClipboardNotifyType {
    QEMU_CLIPBOARD_UPDATE_INFO,
    QEMU_CLIPBOARD_RESET_SERIAL,
};

struct QemuClipboardNotify {
    QemuClipboardNotifyType type;
    union {
        QemuClipboardInfo *info;
    };
};

struct QemuClipboardInfo {
    uint32_t refcount;
    QemuClipboardPeer *owner;
    QemuClipboardSelection selection;
    bool has_serial;
    uint32_t serial;
    struct {
        bool available;
        bool requested;
        size_t size;
        void *data;
    } types[QEMU_CLIPBOARD_TYPE__COUNT];
};

void qemu_clipboard_peer_register(QemuClipboardPeer *peer);

void qemu_clipboard_peer_unregister(QemuClipboardPeer *peer);

bool qemu_clipboard_peer_owns(QemuClipboardPeer *peer,
                              QemuClipboardSelection selection);

void qemu_clipboard_peer_release(QemuClipboardPeer *peer,
                                 QemuClipboardSelection selection);

QemuClipboardInfo *qemu_clipboard_info(QemuClipboardSelection selection);

bool qemu_clipboard_check_serial(QemuClipboardInfo *info, bool client);

QemuClipboardInfo *qemu_clipboard_info_new(QemuClipboardPeer *owner,
                                           QemuClipboardSelection selection);
QemuClipboardInfo *qemu_clipboard_info_ref(QemuClipboardInfo *info);

void qemu_clipboard_info_unref(QemuClipboardInfo *info);

void qemu_clipboard_update(QemuClipboardInfo *info);

void qemu_clipboard_reset_serial(void);

void qemu_clipboard_request(QemuClipboardInfo *info,
                            QemuClipboardType type);

void qemu_clipboard_set_data(QemuClipboardPeer *peer,
                             QemuClipboardInfo *info,
                             QemuClipboardType type,
                             uint32_t size,
                             const void *data,
                             bool update);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QemuClipboardInfo, qemu_clipboard_info_unref)

#endif /* QEMU_CLIPBOARD_H */
