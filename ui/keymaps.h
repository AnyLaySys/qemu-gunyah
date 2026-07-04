
#ifndef QEMU_KEYMAPS_H
#define QEMU_KEYMAPS_H

#include "ui/kbd-state.h"

typedef struct {
    const char* name;
    int keysym;
} name2keysym_t;

#define SCANCODE_KEYMASK 0xff
#define SCANCODE_KEYCODEMASK 0x7f

#define SCANCODE_GREY   0x80
#define SCANCODE_EMUL0  0xE0
#define SCANCODE_EMUL1  0xE1
#define SCANCODE_UP     0x80

#define SCANCODE_SHIFT  0x100
#define SCANCODE_CTRL   0x200
#define SCANCODE_ALT    0x400
#define SCANCODE_ALTGR  0x800

typedef struct kbd_layout_t kbd_layout_t;

kbd_layout_t *init_keyboard_layout(const name2keysym_t *table,
                                   const char *language, Error **errp);
int keysym2scancode(kbd_layout_t *k, int keysym,
                    QKbdState *kbd, bool down);
int keycode_is_keypad(kbd_layout_t *k, int keycode);
int keysym_is_numlock(kbd_layout_t *k, int keysym);

#endif /* QEMU_KEYMAPS_H */
