#ifndef EGL_HELPERS_H
#define EGL_HELPERS_H

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include "ui/console.h"
#include "ui/shader.h"

extern EGLDisplay *qemu_egl_display;
extern EGLConfig qemu_egl_config;
extern EGLContext qemu_egl_rn_ctx;
extern EGLSurface qemu_egl_rn_surface;

typedef struct egl_fb {
    int width;
    int height;
    GLuint texture;
} egl_fb;

#define EGL_FB_INIT { 0 }

void egl_fb_setup_default(egl_fb *fb, int width, int height);
void egl_texture_blend(QemuGLShader *gls, egl_fb *dst, egl_fb *src,
                       int x, int y, double scale_x, double scale_y);
bool egl_init(const char *rendernode, DisplayGLMode mode, Error **errp);
const char *qemu_egl_get_error_string(void);

#endif
