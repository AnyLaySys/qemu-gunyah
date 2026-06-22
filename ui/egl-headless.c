#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "ui/console.h"
#include "ui/egl-helpers.h"
#include "ui/egl-context.h"
#include "ui/shader.h"

typedef struct egl_dpy {
    DisplayChangeListener dcl;
    DisplaySurface *ds;
    QemuGLShader *gls;
    egl_fb guest_fb;
    egl_fb cursor_fb;
    egl_fb blit_fb;
    bool y_0_top;
    uint32_t pos_x;
    uint32_t pos_y;
#ifdef __ANDROID__
    QEMUCursor *cursor;
#endif
} egl_dpy;

/* ------------------------------------------------------------------ */

static void egl_refresh(DisplayChangeListener *dcl)
{
    graphic_hw_update(dcl->con);
}

static void egl_gfx_update(DisplayChangeListener *dcl,
                           int x, int y, int w, int h)
{
}

static void egl_gfx_switch(DisplayChangeListener *dcl,
                           struct DisplaySurface *new_surface)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    edpy->ds = new_surface;
}

static QEMUGLContext egl_create_context(DisplayGLCtx *dgc,
                                        QEMUGLParams *params)
{
    eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   qemu_egl_rn_ctx);
    return qemu_egl_create_context(dgc, params);
}

static void egl_scanout_disable(DisplayChangeListener *dcl)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    egl_fb_destroy(&edpy->guest_fb);
    egl_fb_destroy(&edpy->blit_fb);
}

static void egl_scanout_texture(DisplayChangeListener *dcl,
                                uint32_t backing_id,
                                bool backing_y_0_top,
                                uint32_t backing_width,
                                uint32_t backing_height,
                                uint32_t x, uint32_t y,
                                uint32_t w, uint32_t h,
                                void *d3d_tex2d)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    edpy->y_0_top = backing_y_0_top;

    /* source framebuffer */
    egl_fb_setup_for_tex(&edpy->guest_fb,
                         backing_width, backing_height, backing_id, false);

    /* dest framebuffer */
    if (edpy->blit_fb.width  != backing_width ||
        edpy->blit_fb.height != backing_height) {
        egl_fb_destroy(&edpy->blit_fb);
        egl_fb_setup_new_tex(&edpy->blit_fb, backing_width, backing_height);
    }
}

#ifdef CONFIG_GBM

static void egl_scanout_dmabuf(DisplayChangeListener *dcl,
                               QemuDmaBuf *dmabuf)
{
    uint32_t width, height, texture;

    egl_dmabuf_import_texture(dmabuf);
    texture = qemu_dmabuf_get_texture(dmabuf);
    if (!texture) {
        return;
    }

    width = qemu_dmabuf_get_width(dmabuf);
    height = qemu_dmabuf_get_height(dmabuf);

    egl_scanout_texture(dcl, texture, false, width, height, 0, 0,
                        width, height, NULL);
}

static void egl_cursor_dmabuf(DisplayChangeListener *dcl,
                              QemuDmaBuf *dmabuf, bool have_hot,
                              uint32_t hot_x, uint32_t hot_y)
{
    uint32_t width, height, texture;
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    if (dmabuf) {
        egl_dmabuf_import_texture(dmabuf);
        texture = qemu_dmabuf_get_texture(dmabuf);
        if (!texture) {
            return;
        }

        width = qemu_dmabuf_get_width(dmabuf);
        height = qemu_dmabuf_get_height(dmabuf);
        egl_fb_setup_for_tex(&edpy->cursor_fb, width, height, texture, false);
    } else {
        egl_fb_destroy(&edpy->cursor_fb);
    }
}

static void egl_release_dmabuf(DisplayChangeListener *dcl,
                               QemuDmaBuf *dmabuf)
{
    egl_dmabuf_release_texture(dmabuf);
}

#endif

static void egl_cursor_position(DisplayChangeListener *dcl,
                                uint32_t pos_x, uint32_t pos_y)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    edpy->pos_x = pos_x;
    edpy->pos_y = pos_y;
}

#ifdef __ANDROID__
static void egl_cursor_define(DisplayChangeListener *dcl,
                              QEMUCursor *cursor)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    cursor_unref(edpy->cursor);
    edpy->cursor = cursor_ref(cursor);
}

static void egl_mouse_set(DisplayChangeListener *dcl,
                           int x, int y, bool on)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    edpy->pos_x = x;
    edpy->pos_y = y;
}

/* Software cursor compositing: blend RGBA cursor onto BGRA surface */
static void egl_blend_cursor(egl_dpy *edpy)
{
    QEMUCursor *c = edpy->cursor;
    DisplaySurface *ds = edpy->ds;
    uint32_t *dst, *src;
    int sw, sh, cx, cy, cw, ch;
    int x, y;

    if (!c || !ds) {
        return;
    }

    sw = surface_width(ds);
    sh = surface_height(ds);
    cx = edpy->pos_x;
    cy = edpy->pos_y;
    cw = c->width;
    ch = c->height;
    dst = (uint32_t *)surface_data(ds);
    src = c->data;

    for (y = 0; y < ch; y++) {
        int dy = cy + y;
        if (dy < 0 || dy >= sh) continue;
        for (x = 0; x < cw; x++) {
            int dx = cx + x;
            if (dx < 0 || dx >= sw) continue;

            uint32_t sp = src[y * cw + x];
            uint32_t sa = (sp >> 24) & 0xFF;
            if (sa == 0) continue;

            uint32_t dp = dst[dy * sw + dx];
            if (sa == 0xFF) {
                /* Cursor pixel is ARGB, surface is BGRX after our swizzle.
                 * Swap R and B from cursor to match surface format. */
                dst[dy * sw + dx] = ((sp & 0x00FF0000) >> 16) |
                                     (sp & 0xFF00FF00) |
                                    ((sp & 0x000000FF) << 16);
            } else {
                /* Alpha blend with R<->B swap */
                uint32_t sr = (sp >> 16) & 0xFF;
                uint32_t sg = (sp >> 8) & 0xFF;
                uint32_t sb = sp & 0xFF;
                uint32_t dr = dp & 0xFF;         /* B in BGRX = R display */
                uint32_t dg = (dp >> 8) & 0xFF;
                uint32_t db = (dp >> 16) & 0xFF;  /* R in BGRX = B display */
                uint32_t da = sa;
                uint32_t ia = 255 - sa;
                dr = (sr * da + dr * ia) / 255;
                dg = (sg * da + dg * ia) / 255;
                db = (sb * da + db * ia) / 255;
                dst[dy * sw + dx] = (dr) | (dg << 8) | (db << 16) | 0xFF000000;
            }
        }
    }
}
#endif

static void egl_scanout_flush(DisplayChangeListener *dcl,
                              uint32_t x, uint32_t y,
                              uint32_t w, uint32_t h)
{
    egl_dpy *edpy = container_of(dcl, egl_dpy, dcl);

    if (!edpy->guest_fb.texture || !edpy->ds) {
        return;
    }

    /* Drain any prior GL errors from virglrenderer */
    while (glGetError() != GL_NO_ERROR) {}

    assert(surface_format(edpy->ds) == PIXMAN_x8r8g8b8);

#ifdef __ANDROID__
    /*
     * On Android/GLES, skip the intermediate blit entirely.
     * The blit uses VAOs and shader programs that were created in the
     * render-node EGL context, but flush runs in virglrenderer's context
     * where those VAOs don't exist. Instead, read directly from the
     * guest framebuffer with Y-flip handling in software.
     */
    /* Ensure all virglrenderer GL commands are completed before readback. */
    glFinish();

#ifdef __ANDROID__
    /* On Adreno GLES, we must use virglrenderer's own FBO to read the
     * texture content. Virglrenderer uses FBO 1 for rendering.
     * First try FBO 1, then fall back to re-attaching to our FBO. */
    {
        /* Bind virglrenderer's FBO and check if our texture is attached */
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 1);
        GLint attached_tex = 0;
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
            &attached_tex);

        if ((GLuint)attached_tex == edpy->guest_fb.texture) {
            /* Great - virgl's FBO already has our texture, read from it */
            edpy->guest_fb.framebuffer = 1;
        } else {
            /* Texture not attached to FBO 1, re-attach to our FBO.
             * Also try attaching our texture to FBO 1. */
            glBindFramebuffer(GL_FRAMEBUFFER, 1);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, edpy->guest_fb.texture, 0);
            edpy->guest_fb.framebuffer = 1;
        }
        while (glGetError() != GL_NO_ERROR) {}
    }
#else
    glBindFramebuffer(GL_FRAMEBUFFER, edpy->guest_fb.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, edpy->guest_fb.texture, 0);
#endif

    egl_fb_read_flipped(edpy->ds, &edpy->guest_fb, edpy->y_0_top);

    /* Don't software-blend cursor; display backends handle it separately. */

    (void)glGetError(); /* consume any read error */
#else
    if (edpy->cursor_fb.texture) {
        /* have cursor -> render using textures */
        egl_texture_blit(edpy->gls, &edpy->blit_fb, &edpy->guest_fb,
                         !edpy->y_0_top);
        egl_texture_blend(edpy->gls, &edpy->blit_fb, &edpy->cursor_fb,
                          !edpy->y_0_top, edpy->pos_x, edpy->pos_y,
                          1.0, 1.0);
    } else {
        /* no cursor -> use simple framebuffer blit */
        egl_fb_blit(&edpy->blit_fb, &edpy->guest_fb, edpy->y_0_top);
    }

    egl_fb_read(edpy->ds, &edpy->blit_fb);
#endif

    dpy_gfx_update(edpy->dcl.con, x, y, w, h);
}

static const DisplayChangeListenerOps egl_ops = {
    .dpy_name                = "egl-headless",
    .dpy_refresh             = egl_refresh,
    .dpy_gfx_update          = egl_gfx_update,
    .dpy_gfx_switch          = egl_gfx_switch,

    .dpy_gl_scanout_disable  = egl_scanout_disable,
    .dpy_gl_scanout_texture  = egl_scanout_texture,
#ifdef CONFIG_GBM
    .dpy_gl_scanout_dmabuf   = egl_scanout_dmabuf,
    .dpy_gl_cursor_dmabuf    = egl_cursor_dmabuf,
    .dpy_gl_release_dmabuf   = egl_release_dmabuf,
#endif
    .dpy_gl_cursor_position  = egl_cursor_position,
    .dpy_gl_update           = egl_scanout_flush,
#ifdef __ANDROID__
    .dpy_cursor_define       = egl_cursor_define,
    .dpy_mouse_set           = egl_mouse_set,
#endif
};

static bool
egl_is_compatible_dcl(DisplayGLCtx *dgc,
                      DisplayChangeListener *dcl)
{
    if (!dcl->ops->dpy_gl_update) {
        /*
         * egl-headless is compatible with all 2d listeners, as it blits the GL
         * updates on the 2d console surface.
         */
        return true;
    }

    return dcl->ops == &egl_ops;
}

static const DisplayGLCtxOps eglctx_ops = {
    .dpy_gl_ctx_is_compatible_dcl = egl_is_compatible_dcl,
    .dpy_gl_ctx_create       = egl_create_context,
    .dpy_gl_ctx_destroy      = qemu_egl_destroy_context,
    .dpy_gl_ctx_make_current = qemu_egl_make_context_current,
};

static void early_egl_headless_init(DisplayOptions *opts)
{
    DisplayGLMode mode = DISPLAY_GL_MODE_ON;

    if (opts->has_gl) {
        mode = opts->gl;
    }

    egl_init(opts->u.egl_headless.rendernode, mode, &error_fatal);
}

static void egl_headless_init(DisplayState *ds, DisplayOptions *opts)
{
    QemuConsole *con;
    egl_dpy *edpy;
    int idx;

    for (idx = 0;; idx++) {
        DisplayGLCtx *ctx;

        con = qemu_console_lookup_by_index(idx);
        if (!con || !qemu_console_is_graphic(con)) {
            break;
        }

        edpy = g_new0(egl_dpy, 1);
        edpy->dcl.con = con;
        edpy->dcl.ops = &egl_ops;
        edpy->gls = qemu_gl_init_shader();
        ctx = g_new0(DisplayGLCtx, 1);
        ctx->ops = &eglctx_ops;
        qemu_console_set_display_gl_ctx(con, ctx);
        register_displaychangelistener(&edpy->dcl);
    }
}

static QemuDisplay qemu_display_egl = {
    .type       = DISPLAY_TYPE_EGL_HEADLESS,
    .early_init = early_egl_headless_init,
    .init       = egl_headless_init,
};

static void register_egl(void)
{
    qemu_display_register(&qemu_display_egl);
}

type_init(register_egl);

module_dep("ui-opengl");
