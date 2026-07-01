#include "qemu/osdep.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "qapi/error.h"
#include "system/system.h"
#include "ui/console.h"
#include "ui/input.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define AGL_MAGIC 0x41474c31u
#define AGL_PORT 7799
#define AGL_MSG_FRAME 1
#define AGL_MSG_POINTER 2
#define AGL_MSG_KEY 3
#define AGL_MSG_SCROLL 4
#define AGL_HDR_WORDS 6
#define AGL_HDR_SIZE (AGL_HDR_WORDS * sizeof(uint32_t))

typedef struct AglConsole {
    DisplayChangeListener dcl;
    DisplaySurface *surface;
    int fd;
    int buttons;
    int cursor_x;
    int cursor_y;
    bool cursor_on;
    bool dirty;
    gint64 reconnect_us;
    QEMUCursor *cursor;
    GByteArray *rgba;
    uint8_t rx[AGL_HDR_SIZE * 16];
    size_t rx_len;
} AglConsole;

static AglConsole *agl_console;
static int agl_num_outputs;

static void agl_close(AglConsole *ac)
{
    if (ac->fd >= 0) {
        close(ac->fd);
        ac->fd = -1;
    }
    ac->rx_len = 0;
}

static bool agl_connect(AglConsole *ac)
{
    struct sockaddr_in addr;
    int one = 1;
    int fd;
    gint64 now = g_get_monotonic_time();

    if (ac->fd >= 0) {
        return true;
    }
    if (now < ac->reconnect_us) {
        return false;
    }
    ac->reconnect_us = now + G_USEC_PER_SEC;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(AGL_PORT);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return false;
    }

    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    ac->fd = fd;
    return true;
}

static bool agl_write_all(AglConsole *ac, const void *data, size_t size)
{
    const uint8_t *p = data;

    while (size) {
        ssize_t n = send(ac->fd, p, size, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            agl_close(ac);
            return false;
        }
        if (n == 0) {
            agl_close(ac);
            return false;
        }
        p += n;
        size -= n;
    }
    return true;
}

static void agl_send_key(AglConsole *ac, int scan, bool down)
{
    QKeyCode qcode;

    if (scan <= 0 || scan >= qemu_input_map_linux_to_qcode_len) {
        return;
    }
    qcode = qemu_input_map_linux_to_qcode[scan];
    if (qcode) {
        qemu_input_event_send_key_qcode(ac->dcl.con, qcode, down);
    }
}

static void agl_send_pointer(AglConsole *ac, int x, int y, int buttons)
{
    static uint32_t bmap[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT] = 1,
        [INPUT_BUTTON_RIGHT] = 2,
        [INPUT_BUTTON_MIDDLE] = 4,
        [INPUT_BUTTON_SIDE] = 8,
        [INPUT_BUTTON_EXTRA] = 16,
    };
    int width;
    int height;

    if (!ac->surface) {
        return;
    }

    if (ac->buttons != buttons) {
        qemu_input_update_buttons(ac->dcl.con, bmap, ac->buttons, buttons);
        ac->buttons = buttons;
    }

    width = surface_width(ac->surface);
    height = surface_height(ac->surface);
    qemu_input_queue_abs(ac->dcl.con, INPUT_AXIS_X, x, 0, width);
    qemu_input_queue_abs(ac->dcl.con, INPUT_AXIS_Y, y, 0, height);
    qemu_input_event_sync();
}

static void agl_pulse_button(AglConsole *ac, InputButton btn)
{
    qemu_input_queue_btn(ac->dcl.con, btn, true);
    qemu_input_event_sync();
    qemu_input_queue_btn(ac->dcl.con, btn, false);
    qemu_input_event_sync();
}

static void agl_send_scroll(AglConsole *ac, int x, int y)
{
    if (y > 0) {
        agl_pulse_button(ac, INPUT_BUTTON_WHEEL_UP);
    } else if (y < 0) {
        agl_pulse_button(ac, INPUT_BUTTON_WHEEL_DOWN);
    }
    if (x < 0) {
        agl_pulse_button(ac, INPUT_BUTTON_WHEEL_RIGHT);
    } else if (x > 0) {
        agl_pulse_button(ac, INPUT_BUTTON_WHEEL_LEFT);
    }
}

static void agl_handle_msg(AglConsole *ac, uint32_t *w)
{
    uint32_t type = ntohl(w[1]);
    int a = (int)ntohl(w[2]);
    int b = (int)ntohl(w[3]);
    int c = (int)ntohl(w[4]);

    switch (type) {
    case AGL_MSG_POINTER:
        agl_send_pointer(ac, a, b, c);
        break;
    case AGL_MSG_KEY:
        agl_send_key(ac, a, b != 0);
        break;
    case AGL_MSG_SCROLL:
        agl_send_scroll(ac, a, b);
        break;
    default:
        break;
    }
}

static void agl_read_input(AglConsole *ac)
{
    for (;;) {
        ssize_t n;

        if (sizeof(ac->rx) == ac->rx_len) {
            ac->rx_len = 0;
        }

        n = recv(ac->fd, ac->rx + ac->rx_len, sizeof(ac->rx) - ac->rx_len,
                 MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            agl_close(ac);
            break;
        }
        if (n == 0) {
            agl_close(ac);
            break;
        }
        ac->rx_len += n;
    }

    while (ac->rx_len >= AGL_HDR_SIZE) {
        uint32_t words[AGL_HDR_WORDS];

        memcpy(words, ac->rx, AGL_HDR_SIZE);
        if (ntohl(words[0]) != AGL_MAGIC) {
            ac->rx_len = 0;
            return;
        }
        agl_handle_msg(ac, words);
        ac->rx_len -= AGL_HDR_SIZE;
        memmove(ac->rx, ac->rx + AGL_HDR_SIZE, ac->rx_len);
    }
}

static void agl_surface_to_rgba(AglConsole *ac)
{
    DisplaySurface *surface = ac->surface;
    PixelFormat pf = qemu_pixelformat_from_pixman(surface_format(surface));
    int width = surface_width(surface);
    int height = surface_height(surface);
    int stride = surface_stride(surface);
    int bpp = surface_bytes_per_pixel(surface);
    uint8_t *src = surface_data(surface);
    size_t size = (size_t)width * height * 4;
    uint8_t *dst;

    g_byte_array_set_size(ac->rgba, size);
    dst = ac->rgba->data;

    for (int y = 0; y < height; y++) {
        uint8_t *line = src + y * stride;

        for (int x = 0; x < width; x++) {
            uint32_t p = 0;
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a = 0xff;

            memcpy(&p, line + x * bpp, bpp);
            r = pf.rmax ? (((p & pf.rmask) >> pf.rshift) * 255 / pf.rmax) : 0;
            g = pf.gmax ? (((p & pf.gmask) >> pf.gshift) * 255 / pf.gmax) : 0;
            b = pf.bmax ? (((p & pf.bmask) >> pf.bshift) * 255 / pf.bmax) : 0;
            if (pf.abits && pf.amax) {
                a = ((p & pf.amask) >> pf.ashift) * 255 / pf.amax;
            }
            *dst++ = r;
            *dst++ = g;
            *dst++ = b;
            *dst++ = a;
        }
    }
}

static void agl_blend_cursor(AglConsole *ac)
{
    DisplaySurface *surface = ac->surface;
    QEMUCursor *cursor = ac->cursor;
    int width;
    int height;
    int left;
    int top;

    if (!surface || !cursor || !ac->cursor_on) {
        return;
    }

    width = surface_width(surface);
    height = surface_height(surface);
    left = ac->cursor_x - cursor->hot_x;
    top = ac->cursor_y - cursor->hot_y;

    for (int cy = 0; cy < cursor->height; cy++) {
        int sy = top + cy;

        if (sy < 0 || sy >= height) {
            continue;
        }
        for (int cx = 0; cx < cursor->width; cx++) {
            int sx = left + cx;
            uint32_t pixel;
            uint8_t a;
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t *dst;

            if (sx < 0 || sx >= width) {
                continue;
            }

            pixel = cursor->data[cy * cursor->width + cx];
            a = pixel >> 24;
            if (!a) {
                continue;
            }

            r = (pixel >> 16) & 0xff;
            g = (pixel >> 8) & 0xff;
            b = pixel & 0xff;
            dst = ac->rgba->data + (((size_t)sy * width + sx) * 4);

            if (a == 0xff) {
                dst[0] = r;
                dst[1] = g;
                dst[2] = b;
                dst[3] = 0xff;
            } else {
                dst[0] = (r * a + dst[0] * (0xff - a) + 127) / 0xff;
                dst[1] = (g * a + dst[1] * (0xff - a) + 127) / 0xff;
                dst[2] = (b * a + dst[2] * (0xff - a) + 127) / 0xff;
                dst[3] = 0xff;
            }
        }
    }
}

static void agl_send_frame(AglConsole *ac)
{
    uint32_t hdr[AGL_HDR_WORDS];
    int width;
    int height;

    if (!ac->surface || surface_is_placeholder(ac->surface)) {
        return;
    }
    if (!agl_connect(ac)) {
        return;
    }
    agl_read_input(ac);
    if (ac->fd < 0) {
        return;
    }

    width = surface_width(ac->surface);
    height = surface_height(ac->surface);
    agl_surface_to_rgba(ac);
    agl_blend_cursor(ac);

    hdr[0] = htonl(AGL_MAGIC);
    hdr[1] = htonl(AGL_MSG_FRAME);
    hdr[2] = htonl(width);
    hdr[3] = htonl(height);
    hdr[4] = htonl(width * 4);
    hdr[5] = htonl(ac->rgba->len);

    if (agl_write_all(ac, hdr, sizeof(hdr))) {
        agl_write_all(ac, ac->rgba->data, ac->rgba->len);
    }
}

static void agl_gfx_update(DisplayChangeListener *dcl, int x, int y, int w, int h)
{
    AglConsole *ac = container_of(dcl, AglConsole, dcl);

    ac->dirty = true;
}

static void agl_gfx_switch(DisplayChangeListener *dcl, DisplaySurface *surface)
{
    AglConsole *ac = container_of(dcl, AglConsole, dcl);

    ac->surface = surface;
    ac->dirty = true;
}

static void agl_mouse_set(DisplayChangeListener *dcl, int x, int y, bool on)
{
    AglConsole *ac = container_of(dcl, AglConsole, dcl);

    ac->cursor_x = x;
    ac->cursor_y = y;
    ac->cursor_on = on;
    ac->dirty = true;
}

static void agl_cursor_define(DisplayChangeListener *dcl, QEMUCursor *cursor)
{
    AglConsole *ac = container_of(dcl, AglConsole, dcl);

    cursor_unref(ac->cursor);
    ac->cursor = cursor ? cursor_ref(cursor) : NULL;
    ac->dirty = true;
}

static void agl_refresh(DisplayChangeListener *dcl)
{
    AglConsole *ac = container_of(dcl, AglConsole, dcl);

    if (agl_connect(ac)) {
        agl_read_input(ac);
    }
    graphic_hw_update(dcl->con);
    if (ac->dirty) {
        ac->dirty = false;
        agl_send_frame(ac);
    }
}

static const DisplayChangeListenerOps agl_dcl_ops = {
    .dpy_name = "agl",
    .dpy_refresh = agl_refresh,
    .dpy_gfx_update = agl_gfx_update,
    .dpy_gfx_switch = agl_gfx_switch,
    .dpy_gfx_check_format = qemu_pixman_check_format,
    .dpy_mouse_set = agl_mouse_set,
    .dpy_cursor_define = agl_cursor_define,
};

static void agl_display_early_init(DisplayOptions *o)
{
    assert(o->type == DISPLAY_TYPE_AGL);
    o->has_gl = true;
    o->gl = DISPLAY_GL_MODE_ES;
    display_opengl = 1;
}

static void agl_display_init(DisplayState *ds, DisplayOptions *o)
{
    int i;

    assert(o->type == DISPLAY_TYPE_AGL);

    for (i = 0;; i++) {
        QemuConsole *con = qemu_console_lookup_by_index(i);
        if (!con) {
            break;
        }
    }
    agl_num_outputs = i;
    if (agl_num_outputs == 0) {
        return;
    }

    agl_console = g_new0(AglConsole, agl_num_outputs);
    for (i = 0; i < agl_num_outputs; i++) {
        QemuConsole *con = qemu_console_lookup_by_index(i);

        agl_console[i].fd = -1;
        agl_console[i].dcl.ops = &agl_dcl_ops;
        agl_console[i].dcl.con = con;
        agl_console[i].rgba = g_byte_array_new();
        register_displaychangelistener(&agl_console[i].dcl);
    }
}

static QemuDisplay qemu_display_agl = {
    .type = DISPLAY_TYPE_AGL,
    .early_init = agl_display_early_init,
    .init = agl_display_init,
};

static void register_agl(void)
{
    qemu_display_register(&qemu_display_agl);
}

type_init(register_agl);
