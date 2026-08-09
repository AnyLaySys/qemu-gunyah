#include "qemu/osdep.h"

#ifdef __ANDROID__

#include <android/native_window.h>
#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "system/runstate.h"
#include "system/system.h"
#include "ui/agl.h"
#include "ui/console.h"
#include "ui/egl-helpers.h"
#include "ui/input.h"
#include "ui/shader.h"

typedef enum AGLInputType {
    AGL_INPUT_POINTER,
    AGL_INPUT_SCROLL,
    AGL_INPUT_KEY,
    AGL_INPUT_SHUTDOWN,
} AGLInputType;

typedef struct AGLInputEvent {
    AGLInputType type;
    float x;
    float y;
    int value;
    bool down;
    QTAILQ_ENTRY(AGLInputEvent) next;
} AGLInputEvent;

typedef QTAILQ_HEAD(AGLInputQueue, AGLInputEvent) AGLInputQueue;

typedef struct AGLState {
    DisplayChangeListener dcl;
    DisplayGLCtx dgc;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_cond_t idle;
    pthread_t thread;
    bool thread_started;
    bool stopping;
    bool rendering;
    ANativeWindow *pending_window;
    uint64_t window_serial;
    uint64_t frame_serial;
    uint32_t refresh_rate;
    uint32_t applied_refresh_rate;
    int output_width;
    int output_height;
    int applied_output_width;
    int applied_output_height;
    pixman_image_t *image;
    uint64_t image_serial;
    int dirty_x;
    int dirty_y;
    int dirty_w;
    int dirty_h;
    bool scanout;
    GLuint scanout_texture;
    bool scanout_y0_top;
    int scanout_backing_height;
    int scanout_x;
    int scanout_y;
    int scanout_width;
    int scanout_height;
    GLsync fence;
    QEMUCursor *cursor;
    uint64_t cursor_serial;
    int cursor_x;
    int cursor_y;
    bool cursor_on;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    int source_width;
    int source_height;
    uint32_t buttons;
    int pointer_x;
    int pointer_y;
    AGLInputQueue input_queue;
    bool input_scheduled;
    EGLContext render_context;
    EGLSurface render_surface;
} AGLState;

typedef struct AGLFrame {
    uint64_t serial;
    bool scanout;
    GLuint texture;
    bool y0_top;
    int backing_height;
    int x;
    int y;
    int width;
    int height;
    pixman_image_t *image;
    uint64_t image_serial;
    int dirty_x;
    int dirty_y;
    int dirty_w;
    int dirty_h;
    GLsync fence;
    QEMUCursor *cursor;
    uint64_t cursor_serial;
    int cursor_x;
    int cursor_y;
    bool cursor_on;
} AGLFrame;

typedef struct AGLGLContext {
    EGLContext context;
    EGLSurface surface;
} AGLGLContext;

static AGLState agl = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .idle = PTHREAD_COND_INITIALIZER,
    .render_context = EGL_NO_CONTEXT,
    .render_surface = EGL_NO_SURFACE,
};

static EGLSurface agl_create_pbuffer_surface(void)
{
    static const EGLint attrs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE,
    };

    return eglCreatePbufferSurface(qemu_egl_display, qemu_egl_config, attrs);
}

static void agl_signal_locked(GLsync fence)
{
    if (fence) {
        if (agl.fence) {
            glDeleteSync(agl.fence);
        }
        agl.fence = fence;
    }
    agl.frame_serial++;
    pthread_cond_signal(&agl.cond);
}

static GLsync agl_create_fence(void)
{
    GLsync fence = NULL;

    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
    }
    return fence;
}

static void agl_dirty_locked(int x, int y, int w, int h)
{
    int x2;
    int y2;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (!agl.dirty_w || !agl.dirty_h) {
        agl.dirty_x = x;
        agl.dirty_y = y;
        agl.dirty_w = w;
        agl.dirty_h = h;
        return;
    }
    x2 = MAX(agl.dirty_x + agl.dirty_w, x + w);
    y2 = MAX(agl.dirty_y + agl.dirty_h, y + h);
    agl.dirty_x = MIN(agl.dirty_x, x);
    agl.dirty_y = MIN(agl.dirty_y, y);
    agl.dirty_w = x2 - agl.dirty_x;
    agl.dirty_h = y2 - agl.dirty_y;
}

static void agl_release_surface(EGLSurface *surface, ANativeWindow **window)
{
    eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    if (*surface != EGL_NO_SURFACE) {
        eglDestroySurface(qemu_egl_display, *surface);
        *surface = EGL_NO_SURFACE;
    }
    if (*window) {
        ANativeWindow_release(*window);
        *window = NULL;
    }
}

static void agl_apply_window(EGLSurface *surface, ANativeWindow **window,
                             uint64_t *serial)
{
    ANativeWindow *next = NULL;
    uint64_t next_serial;

    pthread_mutex_lock(&agl.lock);
    next_serial = agl.window_serial;
    if (next_serial != *serial && agl.pending_window) {
        next = agl.pending_window;
        ANativeWindow_acquire(next);
    }
    pthread_mutex_unlock(&agl.lock);

    if (next_serial == *serial) {
        return;
    }

    agl_release_surface(surface, window);
    *serial = next_serial;
    *window = next;
    if (!next) {
        return;
    }

    *surface = eglCreateWindowSurface(qemu_egl_display, qemu_egl_config,
                                      next, NULL);
    if (*surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(qemu_egl_display, *surface, *surface,
                        agl.render_context)) {
        error_report("AGL: EGL window setup failed: %s",
                     qemu_egl_get_error_string());
        agl_release_surface(surface, window);
        return;
    }
    eglSwapInterval(qemu_egl_display, 0);
}

static bool agl_take_frame(AGLFrame *frame)
{
    pthread_mutex_lock(&agl.lock);
    if (!agl.frame_serial) {
        pthread_mutex_unlock(&agl.lock);
        return false;
    }
    frame->serial = agl.frame_serial;
    frame->scanout = agl.scanout;
    frame->texture = agl.scanout_texture;
    frame->y0_top = agl.scanout_y0_top;
    frame->backing_height = agl.scanout_backing_height;
    frame->x = agl.scanout_x;
    frame->y = agl.scanout_y;
    frame->width = agl.scanout_width;
    frame->height = agl.scanout_height;
    frame->image = agl.image ? pixman_image_ref(agl.image) : NULL;
    frame->image_serial = agl.image_serial;
    frame->dirty_x = agl.dirty_x;
    frame->dirty_y = agl.dirty_y;
    frame->dirty_w = agl.dirty_w;
    frame->dirty_h = agl.dirty_h;
    agl.dirty_w = 0;
    agl.dirty_h = 0;
    frame->fence = agl.fence;
    agl.fence = NULL;
    frame->cursor = agl.cursor ? cursor_ref(agl.cursor) : NULL;
    frame->cursor_serial = agl.cursor_serial;
    frame->cursor_x = agl.cursor_x;
    frame->cursor_y = agl.cursor_y;
    frame->cursor_on = agl.cursor_on;
    agl.rendering = true;
    pthread_mutex_unlock(&agl.lock);
    return true;
}

static bool agl_image_format(pixman_format_code_t format, GLenum *gl_format,
                             GLenum *gl_type, GLint *internal,
                             bool *swap_rb, bool *opaque)
{
    switch (format) {
    case PIXMAN_BE_b8g8r8x8:
        *opaque = true;
        *swap_rb = true;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        *internal = GL_RGBA;
        return true;
    case PIXMAN_BE_b8g8r8a8:
        *opaque = false;
        *swap_rb = true;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        *internal = GL_RGBA;
        return true;
    case PIXMAN_BE_x8r8g8b8:
        *opaque = true;
        *swap_rb = false;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        *internal = GL_RGBA;
        return true;
    case PIXMAN_BE_a8r8g8b8:
        *opaque = false;
        *swap_rb = false;
        *gl_format = GL_RGBA;
        *gl_type = GL_UNSIGNED_BYTE;
        *internal = GL_RGBA;
        return true;
    case PIXMAN_r5g6b5:
        *opaque = true;
        *swap_rb = false;
        *gl_format = GL_RGB;
        *gl_type = GL_UNSIGNED_SHORT_5_6_5;
        *internal = GL_RGB;
        return true;
    default:
        return false;
    }
}

static bool agl_upload_image(const AGLFrame *frame, GLuint texture,
                             uint64_t *image_serial)
{
    GLenum format;
    GLenum type;
    GLint internal;
    int width;
    int height;
    int stride;
    int bpp;
    int x;
    int y;
    int w;
    int h;
    uint8_t *data;
    bool swap_rb;
    bool opaque;

    if (!frame->image ||
        !agl_image_format(pixman_image_get_format(frame->image),
                          &format, &type, &internal, &swap_rb, &opaque)) {
        return false;
    }

    width = pixman_image_get_width(frame->image);
    height = pixman_image_get_height(frame->image);
    stride = pixman_image_get_stride(frame->image);
    bpp = PIXMAN_FORMAT_BPP(pixman_image_get_format(frame->image)) / 8;
    data = (uint8_t *)pixman_image_get_data(frame->image);

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp == 2 ? 2 : 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / bpp);
    if (*image_serial != frame->image_serial) {
        glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0,
                     format, type, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R,
                        swap_rb ? GL_BLUE : GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B,
                        swap_rb ? GL_RED : GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A,
                        opaque ? GL_ONE : GL_ALPHA);
        *image_serial = frame->image_serial;
    } else if (frame->dirty_w > 0 && frame->dirty_h > 0) {
        x = MAX(0, frame->dirty_x);
        y = MAX(0, frame->dirty_y);
        w = MIN(frame->dirty_w, width - x);
        h = MIN(frame->dirty_h, height - y);
        if (w > 0 && h > 0) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                            format, type, data + y * stride + x * bpp);
        }
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    return true;
}

static bool agl_gfx_check_format(DisplayChangeListener *dcl,
                                 pixman_format_code_t format)
{
    GLenum gl_format;
    GLenum gl_type;
    GLint internal;
    bool swap_rb;
    bool opaque;

    return agl_image_format(format, &gl_format, &gl_type, &internal,
                            &swap_rb, &opaque);
}

static void agl_viewport(int window_width, int window_height,
                         int source_width, int source_height,
                         int *x, int *y, int *width, int *height)
{
    int64_t scaled_width;
    int64_t scaled_height;

    scaled_width = (int64_t)window_height * source_width / source_height;
    if (scaled_width <= window_width) {
        *width = scaled_width;
        *height = window_height;
    } else {
        scaled_height = (int64_t)window_width * source_height / source_width;
        *width = window_width;
        *height = scaled_height;
    }
    *x = (window_width - *width) / 2;
    *y = (window_height - *height) / 2;
}

static void agl_upload_cursor(QEMUCursor *cursor, GLuint texture,
                              egl_fb *cursor_fb)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cursor->width, cursor->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, cursor->data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    cursor_fb->texture = texture;
    cursor_fb->width = cursor->width;
    cursor_fb->height = cursor->height;
}

static void *agl_render_thread(void *opaque)
{
    EGLSurface surface = EGL_NO_SURFACE;
    ANativeWindow *window = NULL;
    uint64_t window_serial = 0;
    uint64_t rendered_serial = 0;
    uint64_t uploaded_image_serial = 0;
    uint64_t uploaded_cursor_serial = 0;
    GLuint source_fbo = 0;
    GLuint image_texture = 0;
    GLuint cursor_texture = 0;
    egl_fb win_fb = EGL_FB_INIT;
    egl_fb cursor_fb = EGL_FB_INIT;
    QemuGLShader *gls = NULL;
    bool gl_ready = false;

    pthread_setname_np(pthread_self(), "agl-present");

    for (;;) {
        AGLFrame frame = { 0 };
        int window_width;
        int window_height;
        int viewport_x;
        int viewport_y;
        int viewport_width;
        int viewport_height;
        int source_width;
        int source_height;
        int src_x1;
        int src_y1;
        int src_x2;
        int src_y2;
        GLuint source_texture;
        bool stale;

        pthread_mutex_lock(&agl.lock);
        while (!agl.stopping && rendered_serial == agl.frame_serial &&
               window_serial == agl.window_serial) {
            pthread_cond_wait(&agl.cond, &agl.lock);
        }
        if (agl.stopping) {
            pthread_mutex_unlock(&agl.lock);
            break;
        }
        pthread_mutex_unlock(&agl.lock);

        agl_apply_window(&surface, &window, &window_serial);
        if (surface == EGL_NO_SURFACE || !window) {
            GLsync fence;

            pthread_mutex_lock(&agl.lock);
            rendered_serial = agl.frame_serial;
            fence = agl.fence;
            agl.fence = NULL;
            pthread_mutex_unlock(&agl.lock);
            if (fence &&
                eglMakeCurrent(qemu_egl_display, agl.render_surface,
                               agl.render_surface, agl.render_context)) {
                glDeleteSync(fence);
            }
            continue;
        }

        if (!agl_take_frame(&frame)) {
            continue;
        }
        rendered_serial = frame.serial;
        if (!eglMakeCurrent(qemu_egl_display, surface, surface,
                            agl.render_context)) {
            goto frame_done;
        }
        if (!gl_ready) {
            glGenFramebuffers(1, &source_fbo);
            glGenTextures(1, &image_texture);
            glGenTextures(1, &cursor_texture);
            gls = qemu_gl_init_shader();
            gl_ready = true;
        }
        if (frame.fence) {
            glWaitSync(frame.fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(frame.fence);
            frame.fence = NULL;
        }

        window_width = ANativeWindow_getWidth(window);
        window_height = ANativeWindow_getHeight(window);
        if (window_width <= 0 || window_height <= 0) {
            goto frame_done;
        }
        source_texture = frame.texture;
        source_width = frame.width;
        source_height = frame.height;
        src_x1 = frame.x;
        src_x2 = frame.x + frame.width;
        if (frame.y0_top) {
            src_y1 = frame.y;
            src_y2 = frame.y + frame.height;
        } else {
            src_y1 = frame.backing_height - frame.y;
            src_y2 = frame.backing_height - frame.y - frame.height;
        }

        if (!frame.scanout) {
            if (!agl_upload_image(&frame, image_texture,
                                  &uploaded_image_serial)) {
                goto frame_done;
            }
            source_texture = image_texture;
            source_width = pixman_image_get_width(frame.image);
            source_height = pixman_image_get_height(frame.image);
            src_x1 = 0;
            src_x2 = source_width;
            src_y1 = source_height;
            src_y2 = 0;
        } else if (!source_texture || !glIsTexture(source_texture)) {
            goto frame_done;
        }
        if (source_width <= 0 || source_height <= 0) {
            goto frame_done;
        }

        agl_viewport(window_width, window_height, source_width, source_height,
                     &viewport_x, &viewport_y,
                     &viewport_width, &viewport_height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, window_width, window_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (frame.scanout) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, source_fbo);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, source_texture, 0);
            if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) !=
                GL_FRAMEBUFFER_COMPLETE) {
                goto frame_done;
            }
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(src_x1, src_y1, src_x2, src_y2,
                              viewport_x, viewport_y,
                              viewport_x + viewport_width,
                              viewport_y + viewport_height,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(viewport_x, viewport_y,
                       viewport_width, viewport_height);
            glBindTexture(GL_TEXTURE_2D, source_texture);
            qemu_gl_run_texture_blit(gls);
        }

        egl_fb_setup_default(&win_fb, window_width, window_height);
        if (frame.cursor && frame.cursor_on) {
            double scale_x = (double)viewport_width / source_width;
            double scale_y = (double)viewport_height / source_height;
            int cursor_x;
            int cursor_y;

            if (uploaded_cursor_serial != frame.cursor_serial) {
                agl_upload_cursor(frame.cursor, cursor_texture, &cursor_fb);
                uploaded_cursor_serial = frame.cursor_serial;
            }
            cursor_x = viewport_x +
                (frame.cursor_x - frame.cursor->hot_x) * scale_x;
            cursor_y = window_height - viewport_y - viewport_height +
                (frame.cursor_y - frame.cursor->hot_y) * scale_y;
            egl_texture_blend(gls, &win_fb, &cursor_fb,
                              cursor_x, cursor_y, scale_x, scale_y);
        }
        pthread_mutex_lock(&agl.lock);
        stale = frame.serial != agl.frame_serial ||
            window_serial != agl.window_serial;
        pthread_mutex_unlock(&agl.lock);
        if (stale) {
            goto frame_done;
        }
        if (!eglSwapBuffers(qemu_egl_display, surface)) {
            error_report("AGL: EGL swap failed: %s",
                         qemu_egl_get_error_string());
            agl_release_surface(&surface, &window);
        }

        pthread_mutex_lock(&agl.lock);
        agl.viewport_x = viewport_x;
        agl.viewport_y = window_height - viewport_y - viewport_height;
        agl.viewport_width = viewport_width;
        agl.viewport_height = viewport_height;
        agl.source_width = source_width;
        agl.source_height = source_height;
        pthread_mutex_unlock(&agl.lock);

frame_done:
        if (frame.fence) {
            if (eglGetCurrentContext() != EGL_NO_CONTEXT ||
                eglMakeCurrent(qemu_egl_display, agl.render_surface,
                               agl.render_surface, agl.render_context)) {
                glDeleteSync(frame.fence);
            }
        }
        if (frame.image) {
            pixman_image_unref(frame.image);
        }
        cursor_unref(frame.cursor);
        pthread_mutex_lock(&agl.lock);
        agl.rendering = false;
        pthread_cond_broadcast(&agl.idle);
        pthread_mutex_unlock(&agl.lock);
    }

    if (eglMakeCurrent(qemu_egl_display, agl.render_surface,
                       agl.render_surface, agl.render_context)) {
        GLsync fence;

        pthread_mutex_lock(&agl.lock);
        fence = agl.fence;
        agl.fence = NULL;
        pthread_mutex_unlock(&agl.lock);
        if (fence) {
            glDeleteSync(fence);
        }
        if (gl_ready) {
            qemu_gl_fini_shader(gls);
            glDeleteFramebuffers(1, &source_fbo);
            glDeleteTextures(1, &image_texture);
            glDeleteTextures(1, &cursor_texture);
        }
    }
    agl_release_surface(&surface, &window);
    return NULL;
}

static bool agl_gl_is_compatible(DisplayGLCtx *dgc,
                                 DisplayChangeListener *dcl)
{
    return dcl->ops && !strcmp(dcl->ops->dpy_name, "agl");
}

static QEMUGLContext agl_gl_create_context(DisplayGLCtx *dgc,
                                           QEMUGLParams *params)
{
    EGLint attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION_KHR, MIN(params->minor_ver, 2),
        EGL_NONE,
    };
    EGLint fallback[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    AGLGLContext *context = g_new0(AGLGLContext, 1);

    context->context = eglCreateContext(qemu_egl_display, qemu_egl_config,
                                        qemu_egl_rn_ctx, attrs);
    if (context->context == EGL_NO_CONTEXT) {
        context->context = eglCreateContext(qemu_egl_display, qemu_egl_config,
                                            qemu_egl_rn_ctx, fallback);
    }
    if (context->context == EGL_NO_CONTEXT) {
        error_report("AGL: EGL shared context creation failed: %s",
                     qemu_egl_get_error_string());
        g_free(context);
        return NULL;
    }
    context->surface = agl_create_pbuffer_surface();
    if (context->surface == EGL_NO_SURFACE) {
        error_report("AGL: EGL shared pbuffer creation failed: %s",
                     qemu_egl_get_error_string());
        eglDestroyContext(qemu_egl_display, context->context);
        g_free(context);
        return NULL;
    }
    return context;
}

static void agl_gl_destroy_context(DisplayGLCtx *dgc, QEMUGLContext opaque)
{
    AGLGLContext *context = opaque;

    if (context) {
        eglDestroySurface(qemu_egl_display, context->surface);
        eglDestroyContext(qemu_egl_display, context->context);
        g_free(context);
    }
}

static int agl_gl_make_context_current(DisplayGLCtx *dgc,
                                       QEMUGLContext opaque)
{
    AGLGLContext *context = opaque;

    if (!context) {
        return eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE,
                              EGL_NO_SURFACE, EGL_NO_CONTEXT) ? 0 : -1;
    }
    return eglMakeCurrent(qemu_egl_display, context->surface,
                          context->surface, context->context) ? 0 : -1;
}

static void agl_gfx_update(DisplayChangeListener *dcl,
                           int x, int y, int w, int h)
{
    pthread_mutex_lock(&agl.lock);
    agl.scanout = false;
    agl_dirty_locked(x, y, w, h);
    agl_signal_locked(NULL);
    pthread_mutex_unlock(&agl.lock);
}

static void agl_gfx_switch(DisplayChangeListener *dcl,
                           DisplaySurface *surface)
{
    pixman_image_t *image = surface ? pixman_image_ref(surface->image) : NULL;
    pixman_image_t *old;

    pthread_mutex_lock(&agl.lock);
    old = agl.image;
    agl.image = image;
    agl.image_serial++;
    agl.scanout = false;
    if (image) {
        agl_dirty_locked(0, 0, pixman_image_get_width(image),
                         pixman_image_get_height(image));
    }
    agl_signal_locked(NULL);
    pthread_mutex_unlock(&agl.lock);
    if (old) {
        pixman_image_unref(old);
    }
}

static void agl_gl_scanout_disable(DisplayChangeListener *dcl)
{
    pthread_mutex_lock(&agl.lock);
    agl.scanout = false;
    agl.scanout_texture = 0;
    agl_signal_locked(NULL);
    while (agl.rendering) {
        pthread_cond_wait(&agl.idle, &agl.lock);
    }
    pthread_mutex_unlock(&agl.lock);
}

static void agl_gl_scanout_texture(DisplayChangeListener *dcl,
                                   uint32_t backing_id,
                                   bool backing_y0_top,
                                   uint32_t backing_width,
                                   uint32_t backing_height,
                                   uint32_t x, uint32_t y,
                                   uint32_t w, uint32_t h,
                                   void *d3d_tex2d)
{
    GLsync fence = agl_create_fence();

    pthread_mutex_lock(&agl.lock);
    while (agl.rendering && !agl.stopping) {
        pthread_cond_wait(&agl.idle, &agl.lock);
    }
    if (agl.stopping) {
        pthread_mutex_unlock(&agl.lock);
        if (fence) {
            glDeleteSync(fence);
        }
        return;
    }
    agl.scanout = true;
    agl.scanout_texture = backing_id;
    agl.scanout_y0_top = backing_y0_top;
    agl.scanout_backing_height = backing_height;
    agl.scanout_x = x;
    agl.scanout_y = y;
    agl.scanout_width = w;
    agl.scanout_height = h;
    agl_signal_locked(fence);
    pthread_mutex_unlock(&agl.lock);
}

static void agl_gl_update(DisplayChangeListener *dcl,
                          uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    GLsync fence = agl_create_fence();

    pthread_mutex_lock(&agl.lock);
    agl_signal_locked(fence);
    pthread_mutex_unlock(&agl.lock);
}

static void agl_mouse_set(DisplayChangeListener *dcl, int x, int y, bool on)
{
    pthread_mutex_lock(&agl.lock);
    agl.cursor_x = x;
    agl.cursor_y = y;
    agl.cursor_on = on;
    agl_signal_locked(NULL);
    pthread_mutex_unlock(&agl.lock);
}

static void agl_cursor_define(DisplayChangeListener *dcl, QEMUCursor *cursor)
{
    QEMUCursor *old;

    pthread_mutex_lock(&agl.lock);
    old = agl.cursor;
    agl.cursor = cursor ? cursor_ref(cursor) : NULL;
    agl.cursor_serial++;
    agl_signal_locked(NULL);
    pthread_mutex_unlock(&agl.lock);
    cursor_unref(old);
}

static void agl_refresh(DisplayChangeListener *dcl)
{
    uint32_t refresh_rate;
    int output_width;
    int output_height;
    bool refresh_changed;
    bool output_changed;

    graphic_hw_update(dcl->con);
    pthread_mutex_lock(&agl.lock);
    refresh_rate = agl.refresh_rate;
    output_width = agl.output_width;
    output_height = agl.output_height;
    pthread_mutex_unlock(&agl.lock);
    output_changed = output_width > 0 && output_height > 0 &&
        (output_width != agl.applied_output_width ||
         output_height != agl.applied_output_height);
    refresh_changed = refresh_rate &&
        refresh_rate != agl.applied_refresh_rate;
    if (refresh_changed) {
        uint64_t interval = MAX(1ULL, 1000000ULL / refresh_rate);

        update_displaychangelistener(dcl, interval);
        agl.applied_refresh_rate = refresh_rate;
    }
    if ((refresh_changed || output_changed) &&
        dpy_ui_info_supported(dcl->con)) {
        QemuUIInfo info = *dpy_get_ui_info(dcl->con);

        if (refresh_rate) {
            info.refresh_rate = refresh_rate;
        }
        if (output_width > 0 && output_height > 0) {
            info.width = output_width;
            info.height = output_height;
        }
        dpy_set_ui_info(dcl->con, &info, false);
        agl.applied_output_width = output_width;
        agl.applied_output_height = output_height;
    }
}

static const DisplayChangeListenerOps agl_dcl_ops = {
    .dpy_name = "agl",
    .dpy_refresh = agl_refresh,
    .dpy_gfx_update = agl_gfx_update,
    .dpy_gfx_switch = agl_gfx_switch,
    .dpy_gfx_check_format = agl_gfx_check_format,
    .dpy_mouse_set = agl_mouse_set,
    .dpy_cursor_define = agl_cursor_define,
    .dpy_gl_scanout_disable = agl_gl_scanout_disable,
    .dpy_gl_scanout_texture = agl_gl_scanout_texture,
    .dpy_gl_update = agl_gl_update,
};

static const DisplayGLCtxOps agl_gl_ops = {
    .dpy_gl_ctx_is_compatible_dcl = agl_gl_is_compatible,
    .dpy_gl_ctx_create = agl_gl_create_context,
    .dpy_gl_ctx_destroy = agl_gl_destroy_context,
    .dpy_gl_ctx_make_current = agl_gl_make_context_current,
};

static void agl_display_early_init(DisplayOptions *options)
{
    assert(options->type == DISPLAY_TYPE_AGL);
    options->has_gl = true;
    options->gl = DISPLAY_GL_MODE_ES;
    display_opengl = 1;
}

static void agl_display_init(DisplayState *ds, DisplayOptions *options)
{
    EGLint attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    Error *err = NULL;
    QemuConsole *console = NULL;
    int i;

    assert(options->type == DISPLAY_TYPE_AGL);
    QTAILQ_INIT(&agl.input_queue);
    if (!egl_init(NULL, DISPLAY_GL_MODE_ES, &err)) {
        error_report_err(err);
        exit(1);
    }
    agl.render_context = eglCreateContext(qemu_egl_display, qemu_egl_config,
                                          qemu_egl_rn_ctx, attrs);
    if (agl.render_context == EGL_NO_CONTEXT) {
        error_report("AGL: EGL context creation failed: %s",
                     qemu_egl_get_error_string());
        exit(1);
    }
    agl.render_surface = agl_create_pbuffer_surface();
    if (agl.render_surface == EGL_NO_SURFACE) {
        error_report("AGL: EGL pbuffer creation failed: %s",
                     qemu_egl_get_error_string());
        exit(1);
    }

    for (i = 0; ; i++) {
        QemuConsole *candidate = qemu_console_lookup_by_index(i);

        if (!candidate) {
            break;
        }
        if (qemu_console_is_graphic(candidate)) {
            console = candidate;
            break;
        }
    }
    if (!console) {
        return;
    }

    agl.dcl.ops = &agl_dcl_ops;
    agl.dcl.con = console;
    agl.dgc.ops = &agl_gl_ops;
    qemu_console_set_display_gl_ctx(console, &agl.dgc);
    register_displaychangelistener(&agl.dcl);
    if (pthread_create(&agl.thread, NULL, agl_render_thread, NULL)) {
        error_report("AGL: renderer thread creation failed");
        exit(1);
    }
    agl.thread_started = true;
}

static QemuDisplay agl_display = {
    .type = DISPLAY_TYPE_AGL,
    .early_init = agl_display_early_init,
    .init = agl_display_init,
};

static void agl_register(void)
{
    qemu_display_register(&agl_display);
}

type_init(agl_register);

void agl_set_native_window(ANativeWindow *window, uint32_t refresh_rate)
{
    ANativeWindow *old;
    int width = window ? ANativeWindow_getWidth(window) : 0;
    int height = window ? ANativeWindow_getHeight(window) : 0;

    if (window) {
        ANativeWindow_acquire(window);
    }
    pthread_mutex_lock(&agl.lock);
    old = agl.pending_window;
    agl.pending_window = window;
    agl.refresh_rate = refresh_rate;
    agl.output_width = width;
    agl.output_height = height;
    agl.window_serial++;
    agl.frame_serial++;
    pthread_cond_signal(&agl.cond);
    pthread_mutex_unlock(&agl.lock);
    if (old) {
        ANativeWindow_release(old);
    }
    qemu_notify_event();
}

static void agl_process_input_event(AGLInputEvent *event,
                                    QemuConsole *console)
{
    static uint32_t button_map[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT] = 1,
        [INPUT_BUTTON_RIGHT] = 2,
        [INPUT_BUTTON_MIDDLE] = 4,
        [INPUT_BUTTON_SIDE] = 8,
        [INPUT_BUTTON_EXTRA] = 16,
    };

    switch (event->type) {
    case AGL_INPUT_POINTER:
    {
        int x;
        int y;
        int view_x;
        int view_y;
        int view_w;
        int view_h;
        int source_w;
        int source_h;

        pthread_mutex_lock(&agl.lock);
        view_x = agl.viewport_x;
        view_y = agl.viewport_y;
        view_w = agl.viewport_width;
        view_h = agl.viewport_height;
        source_w = agl.source_width;
        source_h = agl.source_height;
        pthread_mutex_unlock(&agl.lock);
        if (!view_w || !view_h || !source_w || !source_h) {
            break;
        }
        x = qemu_input_scale_axis(event->x, view_x, view_x + view_w,
                                  0, source_w);
        y = qemu_input_scale_axis(event->y, view_y, view_y + view_h,
                                  0, source_h);
        x = CLAMP(x, 0, source_w);
        y = CLAMP(y, 0, source_h);
        qemu_input_update_buttons(console, button_map,
                                  agl.buttons, event->value);
        if (qemu_input_is_absolute(console)) {
            qemu_input_queue_abs(console, INPUT_AXIS_X, x, 0, source_w);
            qemu_input_queue_abs(console, INPUT_AXIS_Y, y, 0, source_h);
        } else {
            qemu_input_queue_rel(console, INPUT_AXIS_X, x - agl.pointer_x);
            qemu_input_queue_rel(console, INPUT_AXIS_Y, y - agl.pointer_y);
        }
        agl.buttons = event->value;
        agl.pointer_x = x;
        agl.pointer_y = y;
        qemu_input_event_sync();
        break;
    }
    case AGL_INPUT_SCROLL:
    {
        InputButton vertical = event->y < 0 ? INPUT_BUTTON_WHEEL_UP :
                                              INPUT_BUTTON_WHEEL_DOWN;
        InputButton horizontal = event->x < 0 ? INPUT_BUTTON_WHEEL_LEFT :
                                                INPUT_BUTTON_WHEEL_RIGHT;

        if (event->y) {
            qemu_input_queue_btn(console, vertical, true);
            qemu_input_event_sync();
            qemu_input_queue_btn(console, vertical, false);
            qemu_input_event_sync();
        }
        if (event->x) {
            qemu_input_queue_btn(console, horizontal, true);
            qemu_input_event_sync();
            qemu_input_queue_btn(console, horizontal, false);
            qemu_input_event_sync();
        }
        break;
    }
    case AGL_INPUT_KEY:
    {
        int qcode = qemu_input_linux_to_qcode(event->value);

        if (qcode != Q_KEY_CODE_UNMAPPED) {
            qemu_input_event_send_key_qcode(console, qcode, event->down);
        }
        break;
    }
    case AGL_INPUT_SHUTDOWN:
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
        break;
    }
}

static void agl_input_bh(void *opaque)
{
    AGLInputEvent *event;

    (void)opaque;
    for (;;) {
        QemuConsole *console;

        pthread_mutex_lock(&agl.lock);
        event = QTAILQ_FIRST(&agl.input_queue);
        if (event) {
            QTAILQ_REMOVE(&agl.input_queue, event, next);
        } else {
            agl.input_scheduled = false;
        }
        console = agl.dcl.con;
        pthread_mutex_unlock(&agl.lock);
        if (!event) {
            return;
        }
        if (console) {
            agl_process_input_event(event, console);
        }
        g_free(event);
    }
}

static void agl_queue_input(AGLInputEvent *event)
{
    bool schedule = false;

    pthread_mutex_lock(&agl.lock);
    if (!agl.dcl.con || agl.stopping) {
        pthread_mutex_unlock(&agl.lock);
        g_free(event);
        return;
    }
    QTAILQ_INSERT_TAIL(&agl.input_queue, event, next);
    if (!agl.input_scheduled) {
        agl.input_scheduled = true;
        schedule = true;
    }
    pthread_mutex_unlock(&agl.lock);
    if (schedule) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(), agl_input_bh, NULL);
    }
}

void agl_input_pointer(float x, float y, int buttons)
{
    AGLInputEvent *event = g_new0(AGLInputEvent, 1);

    event->type = AGL_INPUT_POINTER;
    event->x = x;
    event->y = y;
    event->value = buttons;
    agl_queue_input(event);
}

void agl_input_scroll(float x, float y)
{
    AGLInputEvent *event = g_new0(AGLInputEvent, 1);

    event->type = AGL_INPUT_SCROLL;
    event->x = x;
    event->y = y;
    agl_queue_input(event);
}

void agl_input_key(int scan_code, bool down)
{
    AGLInputEvent *event = g_new0(AGLInputEvent, 1);

    event->type = AGL_INPUT_KEY;
    event->value = scan_code;
    event->down = down;
    agl_queue_input(event);
}

void agl_request_shutdown(void)
{
    AGLInputEvent *event = g_new0(AGLInputEvent, 1);

    event->type = AGL_INPUT_SHUTDOWN;
    agl_queue_input(event);
}

void agl_shutdown(void)
{
    ANativeWindow *window;
    pixman_image_t *image;
    QEMUCursor *cursor;
    AGLInputEvent *event;

    pthread_mutex_lock(&agl.lock);
    agl.stopping = true;
    while ((event = QTAILQ_FIRST(&agl.input_queue))) {
        QTAILQ_REMOVE(&agl.input_queue, event, next);
        g_free(event);
    }
    agl.input_scheduled = false;
    pthread_cond_signal(&agl.cond);
    pthread_mutex_unlock(&agl.lock);
    if (agl.thread_started) {
        pthread_join(agl.thread, NULL);
        agl.thread_started = false;
    }
    pthread_mutex_lock(&agl.lock);
    window = agl.pending_window;
    agl.pending_window = NULL;
    image = agl.image;
    agl.image = NULL;
    cursor = agl.cursor;
    agl.cursor = NULL;
    pthread_mutex_unlock(&agl.lock);
    if (window) {
        ANativeWindow_release(window);
    }
    if (image) {
        pixman_image_unref(image);
    }
    cursor_unref(cursor);
    if (qemu_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (agl.render_surface != EGL_NO_SURFACE) {
            eglDestroySurface(qemu_egl_display, agl.render_surface);
            agl.render_surface = EGL_NO_SURFACE;
        }
        if (agl.render_context != EGL_NO_CONTEXT) {
            eglDestroyContext(qemu_egl_display, agl.render_context);
            agl.render_context = EGL_NO_CONTEXT;
        }
        if (qemu_egl_rn_surface != EGL_NO_SURFACE) {
            eglDestroySurface(qemu_egl_display, qemu_egl_rn_surface);
            qemu_egl_rn_surface = EGL_NO_SURFACE;
        }
        if (qemu_egl_rn_ctx != EGL_NO_CONTEXT) {
            eglDestroyContext(qemu_egl_display, qemu_egl_rn_ctx);
            qemu_egl_rn_ctx = EGL_NO_CONTEXT;
        }
    }
}

#endif
