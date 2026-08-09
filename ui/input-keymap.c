#include "qemu/osdep.h"
#include "ui/input.h"

#include "ui/input-keymap-linux-to-qcode.c.inc"
#include "ui/input-keymap-qcode-to-linux.c.inc"
#include "ui/input-keymap-qnum-to-qcode.c.inc"
#include "ui/input-keymap-usb-to-qcode.c.inc"

int qemu_input_linux_to_qcode(unsigned int code)
{
    if (code >= qemu_input_map_linux_to_qcode_len) {
        return 0;
    }
    return qemu_input_map_linux_to_qcode[code];
}

int qemu_input_key_number_to_qcode(unsigned int nr)
{
    if (nr >= qemu_input_map_qnum_to_qcode_len) {
        return 0;
    }
    return qemu_input_map_qnum_to_qcode[nr];
}

int qemu_input_key_value_to_qcode(const KeyValue *value)
{
    if (value->type == KEY_VALUE_KIND_QCODE) {
        return value->u.qcode.data;
    } else {
        assert(value->type == KEY_VALUE_KIND_NUMBER);
        return qemu_input_key_number_to_qcode(value->u.number.data);
    }
}
