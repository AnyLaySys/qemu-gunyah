
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "ui/console.h"
#include "keymaps.h"
#include "ui/input.h"

struct QEMUPutMouseEntry {
    QEMUPutMouseEvent *qemu_put_mouse_event;
    void *qemu_put_mouse_event_opaque;
    int qemu_put_mouse_event_absolute;

    QemuInputHandler h;
    QemuInputHandlerState *s;
    int axis[INPUT_AXIS__MAX];
    int buttons;
};

struct QEMUPutKbdEntry {
    QEMUPutKBDEvent *put_kbd;
    void *opaque;
    QemuInputHandlerState *s;
};

struct QEMUPutLEDEntry {
    QEMUPutLEDEvent *put_led;
    void *opaque;
    QTAILQ_ENTRY(QEMUPutLEDEntry) next;
};

static QTAILQ_HEAD(, QEMUPutLEDEntry) led_handlers =
    QTAILQ_HEAD_INITIALIZER(led_handlers);

static void legacy_mouse_event(DeviceState *dev, QemuConsole *src,
                               InputEvent *evt)
{
    static const int bmap[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT]   = MOUSE_EVENT_LBUTTON,
        [INPUT_BUTTON_MIDDLE] = MOUSE_EVENT_MBUTTON,
        [INPUT_BUTTON_RIGHT]  = MOUSE_EVENT_RBUTTON,
    };
    QEMUPutMouseEntry *s = (QEMUPutMouseEntry *)dev;
    InputBtnEvent *btn;
    InputMoveEvent *move;

    switch (evt->type) {
    case INPUT_EVENT_KIND_BTN:
        btn = evt->u.btn.data;
        if (btn->down) {
            s->buttons |= bmap[btn->button];
        } else {
            s->buttons &= ~bmap[btn->button];
        }
        if (btn->down && btn->button == INPUT_BUTTON_WHEEL_UP) {
            s->qemu_put_mouse_event(s->qemu_put_mouse_event_opaque,
                                    s->axis[INPUT_AXIS_X],
                                    s->axis[INPUT_AXIS_Y],
                                    -1,
                                    s->buttons);
        }
        if (btn->down && btn->button == INPUT_BUTTON_WHEEL_DOWN) {
            s->qemu_put_mouse_event(s->qemu_put_mouse_event_opaque,
                                    s->axis[INPUT_AXIS_X],
                                    s->axis[INPUT_AXIS_Y],
                                    1,
                                    s->buttons);
        }
        if (btn->down && btn->button == INPUT_BUTTON_WHEEL_RIGHT) {
            s->qemu_put_mouse_event(s->qemu_put_mouse_event_opaque,
                                    s->axis[INPUT_AXIS_X],
                                    s->axis[INPUT_AXIS_Y],
                                    -2,
                                    s->buttons);
        }
        if (btn->down && btn->button == INPUT_BUTTON_WHEEL_LEFT) {
            s->qemu_put_mouse_event(s->qemu_put_mouse_event_opaque,
                                    s->axis[INPUT_AXIS_X],
                                    s->axis[INPUT_AXIS_Y],
                                    2,
                                    s->buttons);
        }
        break;
    case INPUT_EVENT_KIND_ABS:
        move = evt->u.abs.data;
        s->axis[move->axis] = move->value;
        break;
    case INPUT_EVENT_KIND_REL:
        move = evt->u.rel.data;
        s->axis[move->axis] += move->value;
        break;
    default:
        break;
    }
}

static void legacy_mouse_sync(DeviceState *dev)
{
    QEMUPutMouseEntry *s = (QEMUPutMouseEntry *)dev;

    s->qemu_put_mouse_event(s->qemu_put_mouse_event_opaque,
                            s->axis[INPUT_AXIS_X],
                            s->axis[INPUT_AXIS_Y],
                            0,
                            s->buttons);

    if (!s->qemu_put_mouse_event_absolute) {
        s->axis[INPUT_AXIS_X] = 0;
        s->axis[INPUT_AXIS_Y] = 0;
    }
}

QEMUPutMouseEntry *qemu_add_mouse_event_handler(QEMUPutMouseEvent *func,
                                                void *opaque, int absolute,
                                                const char *name)
{
    QEMUPutMouseEntry *s;

    s = g_new0(QEMUPutMouseEntry, 1);

    s->qemu_put_mouse_event = func;
    s->qemu_put_mouse_event_opaque = opaque;
    s->qemu_put_mouse_event_absolute = absolute;

    s->h.name = name;
    s->h.mask = INPUT_EVENT_MASK_BTN |
        (absolute ? INPUT_EVENT_MASK_ABS : INPUT_EVENT_MASK_REL);
    s->h.event = legacy_mouse_event;
    s->h.sync = legacy_mouse_sync;
    s->s = qemu_input_handler_register((DeviceState *)s,
                                       &s->h);

    return s;
}

void qemu_activate_mouse_event_handler(QEMUPutMouseEntry *entry)
{
    qemu_input_handler_activate(entry->s);
}

void qemu_remove_mouse_event_handler(QEMUPutMouseEntry *entry)
{
    qemu_input_handler_unregister(entry->s);

    g_free(entry);
}

QEMUPutLEDEntry *qemu_add_led_event_handler(QEMUPutLEDEvent *func,
                                            void *opaque)
{
    QEMUPutLEDEntry *s;

    s = g_new0(QEMUPutLEDEntry, 1);

    s->put_led = func;
    s->opaque = opaque;
    QTAILQ_INSERT_TAIL(&led_handlers, s, next);
    return s;
}

void qemu_remove_led_event_handler(QEMUPutLEDEntry *entry)
{
    if (entry == NULL)
        return;
    QTAILQ_REMOVE(&led_handlers, entry, next);
    g_free(entry);
}

void kbd_put_ledstate(int ledstate)
{
    QEMUPutLEDEntry *cursor;

    QTAILQ_FOREACH(cursor, &led_handlers, next) {
        cursor->put_led(cursor->opaque, ledstate);
    }
}
