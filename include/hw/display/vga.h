#ifndef QEMU_HW_DISPLAY_VGA_H
#define QEMU_HW_DISPLAY_VGA_H

extern bool have_vga;

enum vga_retrace_method {
    VGA_RETRACE_DUMB,
    VGA_RETRACE_PRECISE
};

extern enum vga_retrace_method vga_retrace_method;

#define TYPE_VGA_MMIO "vga-mmio"

#endif
