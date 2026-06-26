
#ifndef UI_SPICE_DISPLAY_H
#define UI_SPICE_DISPLAY_H

#include <spice.h>
#include <spice/ipc_ring.h>
#include <spice/enums.h>
#include <spice/qxl_dev.h>

#include "qemu/thread.h"
#include "ui/qemu-pixman.h"
#include "ui/console.h"

#if defined(CONFIG_OPENGL) && defined(CONFIG_GBM)
#  define HAVE_SPICE_GL 1
#  include "ui/egl-helpers.h"
#  include "ui/egl-context.h"
#endif

#define NUM_MEMSLOTS 8
#define MEMSLOT_GENERATION_BITS 8
#define MEMSLOT_SLOT_BITS 8

#define MEMSLOT_GROUP_HOST  0
#define MEMSLOT_GROUP_GUEST 1
#define NUM_MEMSLOTS_GROUPS 2

typedef enum qxl_async_io {
    QXL_SYNC,
    QXL_ASYNC,
} qxl_async_io;

enum {
    QXL_COOKIE_TYPE_IO,
    QXL_COOKIE_TYPE_RENDER_UPDATE_AREA,
    QXL_COOKIE_TYPE_POST_LOAD_MONITORS_CONFIG,
    QXL_COOKIE_TYPE_GL_DRAW_DONE,
};

typedef struct QXLCookie {
    int      type;
    uint64_t io;
    union {
        uint32_t surface_id;
        QXLRect area;
        struct {
            QXLRect area;
            int redraw;
        } render;
        void *data;
    } u;
} QXLCookie;

QXLCookie *qxl_cookie_new(int type, uint64_t io);

typedef struct SimpleSpiceDisplay SimpleSpiceDisplay;
typedef struct SimpleSpiceUpdate SimpleSpiceUpdate;
typedef struct SimpleSpiceCursor SimpleSpiceCursor;

struct SimpleSpiceDisplay {
    DisplaySurface *ds;
    DisplayGLCtx dgc;
    DisplayChangeListener dcl;
    void *buf;
    int bufsize;
    QXLInstance qxl;
    uint32_t unique;
    pixman_image_t *surface;
    pixman_image_t *mirror;
    int32_t num_surfaces;

    QXLRect dirty;
    int notify;

    QemuMutex lock;
    QTAILQ_HEAD(, SimpleSpiceUpdate) updates;

    SimpleSpiceCursor *ptr_define;
    SimpleSpiceCursor *ptr_move;
    int16_t ptr_x, ptr_y;
    int16_t hot_x, hot_y;

    QEMUCursor *cursor;
    int mouse_x, mouse_y;
    QEMUBH *cursor_bh;

#ifdef HAVE_SPICE_GL
    QEMUBH *gl_unblock_bh;
    QEMUTimer *gl_unblock_timer;
    QemuGLShader *gls;
    int gl_updates;
    bool have_scanout;
    bool have_surface;

    QemuDmaBuf *guest_dmabuf;
    bool guest_dmabuf_refresh;
    bool render_cursor;

    egl_fb guest_fb;
    egl_fb blit_fb;
    egl_fb cursor_fb;
    bool have_hot;
#endif
};

struct SimpleSpiceUpdate {
    QXLDrawable drawable;
    QXLImage image;
    QXLCommandExt ext;
    uint8_t *bitmap;
    QTAILQ_ENTRY(SimpleSpiceUpdate) next;
};

struct SimpleSpiceCursor {
    QXLCursorCmd cmd;
    QXLCommandExt ext;
    QXLCursor cursor;
};

extern bool spice_opengl;

int qemu_spice_rect_is_empty(const QXLRect* r);
void qemu_spice_rect_union(QXLRect *dest, const QXLRect *r);

void qemu_spice_destroy_update(SimpleSpiceDisplay *sdpy, SimpleSpiceUpdate *update);
void qemu_spice_create_host_memslot(SimpleSpiceDisplay *ssd);
void qemu_spice_create_host_primary(SimpleSpiceDisplay *ssd);
void qemu_spice_destroy_host_primary(SimpleSpiceDisplay *ssd);
void qemu_spice_display_init_common(SimpleSpiceDisplay *ssd);

void qemu_spice_display_update(SimpleSpiceDisplay *ssd,
                               int x, int y, int w, int h);
void qemu_spice_display_switch(SimpleSpiceDisplay *ssd,
                               DisplaySurface *surface);
void qemu_spice_display_refresh(SimpleSpiceDisplay *ssd);
void qemu_spice_cursor_refresh_bh(void *opaque);

void qemu_spice_add_memslot(SimpleSpiceDisplay *ssd, QXLDevMemSlot *memslot,
                            qxl_async_io async);
void qemu_spice_del_memslot(SimpleSpiceDisplay *ssd, uint32_t gid,
                            uint32_t sid);
void qemu_spice_create_primary_surface(SimpleSpiceDisplay *ssd, uint32_t id,
                                       QXLDevSurfaceCreate *surface,
                                       qxl_async_io async);
void qemu_spice_destroy_primary_surface(SimpleSpiceDisplay *ssd,
                                        uint32_t id, qxl_async_io async);
void qemu_spice_wakeup(SimpleSpiceDisplay *ssd);
void qemu_spice_display_start(void);
void qemu_spice_display_stop(void);
int qemu_spice_display_is_running(SimpleSpiceDisplay *ssd);

#endif
