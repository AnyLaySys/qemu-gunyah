
#ifndef QEMU_PRINT_H
#define QEMU_PRINT_H

int qemu_vprintf(const char *fmt, va_list ap) G_GNUC_PRINTF(1, 0);
int qemu_printf(const char *fmt, ...) G_GNUC_PRINTF(1, 2);

int qemu_vfprintf(FILE *stream, const char *fmt, va_list ap)
    G_GNUC_PRINTF(2, 0);
int qemu_fprintf(FILE *stream, const char *fmt, ...) G_GNUC_PRINTF(2, 3);

#endif
