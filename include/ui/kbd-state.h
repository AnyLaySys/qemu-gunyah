
#ifndef QEMU_UI_KBD_STATE_H
#define QEMU_UI_KBD_STATE_H

#include "qapi/qapi-types-ui.h"

typedef enum QKbdModifier QKbdModifier;

enum QKbdModifier {
    QKBD_MOD_NONE = 0,

    QKBD_MOD_SHIFT,
    QKBD_MOD_CTRL,
    QKBD_MOD_ALT,
    QKBD_MOD_ALTGR,

    QKBD_MOD_NUMLOCK,
    QKBD_MOD_CAPSLOCK,

    QKBD_MOD__MAX
};

typedef struct QKbdState QKbdState;

QKbdState *qkbd_state_init(QemuConsole *con);

void qkbd_state_free(QKbdState *kbd);

void qkbd_state_key_event(QKbdState *kbd, QKeyCode qcode, bool down);

void qkbd_state_set_delay(QKbdState *kbd, int delay_ms);

bool qkbd_state_key_get(QKbdState *kbd, QKeyCode qcode);

bool qkbd_state_modifier_get(QKbdState *kbd, QKbdModifier mod);

void qkbd_state_lift_all_keys(QKbdState *kbd);

void qkbd_state_switch_console(QKbdState *kbd, QemuConsole *con);

#endif /* QEMU_UI_KBD_STATE_H */
