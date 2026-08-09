#include "qemu/osdep.h"
#include "qapi/error.h"
#include "system/system.h"
#include "ui/egl-helpers.h"

EGLDisplay *qemu_egl_display = EGL_NO_DISPLAY;
EGLConfig qemu_egl_config;
EGLContext qemu_egl_rn_ctx = EGL_NO_CONTEXT;
EGLSurface qemu_egl_rn_surface = EGL_NO_SURFACE;

const char *qemu_egl_get_error_string(void)
{
    switch (eglGetError()) {
    case EGL_SUCCESS:
        return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
        return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
        return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
        return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
        return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
        return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
        return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
        return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
        return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
        return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
        return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
        return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
        return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
        return "EGL_BAD_NATIVE_WINDOW";
    case EGL_CONTEXT_LOST:
        return "EGL_CONTEXT_LOST";
    default:
        return "EGL_UNKNOWN";
    }
}

void egl_fb_setup_default(egl_fb *fb, int width, int height)
{
    fb->width = width;
    fb->height = height;
    fb->texture = 0;
}

void egl_texture_blend(QemuGLShader *gls, egl_fb *dst, egl_fb *src,
                       int x, int y, double scale_x, double scale_y)
{
    int w = scale_x * src->width;
    int h = scale_y * src->height;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(x, dst->height - h - y, w, h);
    glBindTexture(GL_TEXTURE_2D, src->texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    qemu_gl_run_texture_blit(gls);
    glDisable(GL_BLEND);
}

bool egl_init(const char *rendernode, DisplayGLMode mode, Error **errp)
{
    static const EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE,
    };
    static const EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE,
    };
    static const EGLint surface_attrs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE,
    };
    EGLint major;
    EGLint minor;
    EGLint count;

    (void)rendernode;

    if (mode == DISPLAY_GL_MODE_OFF) {
        error_setg(errp, "egl: GL is disabled");
        return false;
    }
    qemu_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (qemu_egl_display == EGL_NO_DISPLAY ||
        !eglInitialize(qemu_egl_display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(qemu_egl_display, config_attrs, &qemu_egl_config,
                         1, &count) || count != 1) {
        error_setg(errp, "egl: initialization failed: %s",
                   qemu_egl_get_error_string());
        return false;
    }
    qemu_egl_rn_ctx = eglCreateContext(qemu_egl_display, qemu_egl_config,
                                       EGL_NO_CONTEXT, context_attrs);
    if (qemu_egl_rn_ctx == EGL_NO_CONTEXT) {
        error_setg(errp, "egl: context creation failed: %s",
                   qemu_egl_get_error_string());
        return false;
    }
    qemu_egl_rn_surface = eglCreatePbufferSurface(qemu_egl_display,
                                                  qemu_egl_config,
                                                  surface_attrs);
    if (qemu_egl_rn_surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(qemu_egl_display, qemu_egl_rn_surface,
                        qemu_egl_rn_surface, qemu_egl_rn_ctx)) {
        error_setg(errp, "egl: pbuffer setup failed: %s",
                   qemu_egl_get_error_string());
        return false;
    }
    display_opengl = 1;
    return true;
}
