
#include "qemu/osdep.h"

#include "io/channel-file.h"
#include "qapi/qapi-commands-ui.h"
#include "qemu/coroutine.h"
#include "trace.h"
#include "ui/console.h"
#ifdef CONFIG_PNG
#include <png.h>
#endif

#ifdef CONFIG_PIXMAN
#ifdef CONFIG_PNG
static bool png_save(int fd, pixman_image_t *image, Error **errp)
{
    int width = pixman_image_get_width(image);
    int height = pixman_image_get_height(image);
    png_struct *png_ptr;
    png_info *info_ptr;
    g_autoptr(pixman_image_t) linebuf =
        qemu_pixman_linebuf_create(PIXMAN_BE_r8g8b8, width);
    uint8_t *buf = (uint8_t *)pixman_image_get_data(linebuf);
    FILE *f = fdopen(fd, "wb");
    int y;
    if (!f) {
        error_setg_errno(errp, errno,
                         "Failed to create file from file descriptor");
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                      NULL, NULL);
    if (!png_ptr) {
        error_setg(errp, "PNG creation failed. Unable to write struct");
        fclose(f);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        error_setg(errp, "PNG creation failed. Unable to write info");
        fclose(f);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_init_io(png_ptr, f);

    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    for (y = 0; y < height; ++y) {
        qemu_pixman_linebuf_fill(linebuf, image, width, 0, y);
        png_write_row(png_ptr, buf);
    }

    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    if (fclose(f) != 0) {
        error_setg_errno(errp, errno,
                         "PNG creation failed. Unable to close file");
        return false;
    }

    return true;
}

#else /* no png support */

static bool png_save(int fd, pixman_image_t *image, Error **errp)
{
    error_setg(errp, "Enable PNG support with libpng for screendump");
    return false;
}

#endif /* CONFIG_PNG */

static bool ppm_save(int fd, pixman_image_t *image, Error **errp)
{
    int width = pixman_image_get_width(image);
    int height = pixman_image_get_height(image);
    g_autoptr(Object) ioc = OBJECT(qio_channel_file_new_fd(fd));
    g_autofree char *header = NULL;
    g_autoptr(pixman_image_t) linebuf = NULL;
    int y;

    trace_ppm_save(fd, image);

    header = g_strdup_printf("P6\n%d %d\n%d\n", width, height, 255);
    if (qio_channel_write_all(QIO_CHANNEL(ioc),
                              header, strlen(header), errp) < 0) {
        return false;
    }

    linebuf = qemu_pixman_linebuf_create(PIXMAN_BE_r8g8b8, width);
    for (y = 0; y < height; y++) {
        qemu_pixman_linebuf_fill(linebuf, image, width, 0, y);
        if (qio_channel_write_all(QIO_CHANNEL(ioc),
                                  (char *)pixman_image_get_data(linebuf),
                                  pixman_image_get_stride(linebuf), errp) < 0) {
            return false;
        }
    }

    return true;
}

void coroutine_fn
qmp_screendump(const char *filename, const char *device,
               bool has_head, int64_t head,
               bool has_format, ImageFormat format, Error **errp)
{
    g_autoptr(pixman_image_t) image = NULL;
    QemuConsole *con;
    DisplaySurface *surface;
    int fd;

    if (device) {
        con = qemu_console_lookup_by_device_name(device, has_head ? head : 0,
                                                 errp);
        if (!con) {
            return;
        }
    } else {
        if (has_head) {
            error_setg(errp, "'head' must be specified together with 'device'");
            return;
        }
        con = qemu_console_lookup_by_index(0);
        if (!con) {
            error_setg(errp, "There is no console to take a screendump from");
            return;
        }
    }

    qemu_console_co_wait_update(con);

    surface = qemu_console_surface(con);
    if (!surface) {
        error_setg(errp, "no surface");
        return;
    }
    image = pixman_image_ref(surface->image);

    fd = qemu_open_old(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd == -1) {
        error_setg(errp, "failed to open file '%s': %s", filename,
                   strerror(errno));
        return;
    }

    if (has_format && format == IMAGE_FORMAT_PNG) {
        if (!png_save(fd, image, errp)) {
            qemu_unlink(filename);
        }
    } else {
        if (!ppm_save(fd, image, errp)) {
            qemu_unlink(filename);
        }
    }
}
#endif /* CONFIG_PIXMAN */
