
#include "qemu/osdep.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/sdl2.h"
#include "trace.h"

void sdl2_process_key(struct sdl2_console *scon,
                      SDL_KeyboardEvent *ev)
{
    int qcode;
    QemuConsole *con = scon->dcl.con;

    if (ev->keysym.scancode >= qemu_input_map_usb_to_qcode_len) {
        return;
    }
    qcode = qemu_input_map_usb_to_qcode[ev->keysym.scancode];
    trace_sdl2_process_key(ev->keysym.scancode, qcode,
                           ev->type == SDL_KEYDOWN ? "down" : "up");
    qkbd_state_key_event(scon->kbd, qcode, ev->type == SDL_KEYDOWN);

    if (QEMU_IS_TEXT_CONSOLE(con)) {
        QemuTextConsole *s = QEMU_TEXT_CONSOLE(con);
        bool ctrl = qkbd_state_modifier_get(scon->kbd, QKBD_MOD_CTRL);
        if (ev->type == SDL_KEYDOWN) {
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

void sdl2_release_modifiers(struct sdl2_console *scon)
{
    qkbd_state_lift_all_keys(scon->kbd);
}
