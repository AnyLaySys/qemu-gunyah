
#ifndef QEMU_X_KEYMAP_H
#define QEMU_X_KEYMAP_H

#include <X11/Xlib.h>

const guint16 *qemu_xkeymap_mapping_table(Display *dpy, size_t *maplen);

#endif
