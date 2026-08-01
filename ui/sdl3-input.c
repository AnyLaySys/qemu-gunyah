#include "qemu/osdep.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/sdl3.h"
#include "trace.h"

void sdl3_process_key(struct sdl3_console *scon,
                      SDL_KeyboardEvent *ev)
{
    int qcode;
    QemuConsole *con = scon->dcl.con;

    if (ev->scancode >= qemu_input_map_usb_to_qcode_len) {
        return;
    }
    qcode = qemu_input_map_usb_to_qcode[ev->scancode];
    trace_sdl3_process_key(ev->scancode, qcode,
                           ev->type == SDL_EVENT_KEY_DOWN ? "down" : "up");
    qkbd_state_key_event(scon->kbd, qcode,
                         ev->type == SDL_EVENT_KEY_DOWN);

    if (QEMU_IS_TEXT_CONSOLE(con)) {
        QemuTextConsole *s = QEMU_TEXT_CONSOLE(con);
        bool ctrl = qkbd_state_modifier_get(scon->kbd, QKBD_MOD_CTRL);
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            switch (qcode) {
            case Q_KEY_CODE_RET:
                qemu_text_console_put_keysym(s, '\n');
                break;
            default:
                qemu_text_console_put_qcode(s, qcode, ctrl);
                break;
            }
        }
    }
}

void sdl3_release_modifiers(struct sdl3_console *scon)
{
    qkbd_state_lift_all_keys(scon->kbd);
}
