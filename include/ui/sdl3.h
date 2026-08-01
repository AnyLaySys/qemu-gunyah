#ifndef QEMU_UI_SDL3_H
#define QEMU_UI_SDL3_H

#undef WIN32_LEAN_AND_MEAN

#include <SDL3/SDL.h>

#include "ui/kbd-state.h"
#ifdef CONFIG_OPENGL
# include "ui/egl-helpers.h"
#endif

struct sdl3_console {
    DisplayGLCtx dgc;
    DisplayChangeListener dcl;
    DisplaySurface *surface;
    DisplayOptions *opts;
    SDL_Texture *texture;
    SDL_Window *real_window;
    SDL_Renderer *real_renderer;
    int idx;
    int last_vm_running; /* per console for caption reasons */
    int hidden;
    int opengl;
    int updates;
    int ignore_hotkeys;
    bool gui_keysym;
    bool render_pending;
    SDL_GLContext winctx;
    QKbdState *kbd;
#ifdef CONFIG_OPENGL
    QemuGLShader *gls;
    egl_fb guest_fb;
    egl_fb win_fb;
    bool y0_top;
    bool scanout_mode;
#endif
};

void sdl3_window_create(struct sdl3_console *scon);
void sdl3_window_destroy(struct sdl3_console *scon);
void sdl3_window_resize(struct sdl3_console *scon);
void sdl3_poll_events(struct sdl3_console *scon);

void sdl3_process_key(struct sdl3_console *scon,
                      SDL_KeyboardEvent *ev);
void sdl3_release_modifiers(struct sdl3_console *scon);

void sdl3_2d_update(DisplayChangeListener *dcl,
                    int x, int y, int w, int h);
void sdl3_2d_switch(DisplayChangeListener *dcl,
                    DisplaySurface *new_surface);
void sdl3_2d_refresh(DisplayChangeListener *dcl);
void sdl3_2d_redraw(struct sdl3_console *scon);
bool sdl3_2d_check_format(DisplayChangeListener *dcl,
                          pixman_format_code_t format);

void sdl3_gl_update(DisplayChangeListener *dcl,
                    int x, int y, int w, int h);
void sdl3_gl_switch(DisplayChangeListener *dcl,
                    DisplaySurface *new_surface);
void sdl3_gl_refresh(DisplayChangeListener *dcl);
void sdl3_gl_redraw(struct sdl3_console *scon);

QEMUGLContext sdl3_gl_create_context(DisplayGLCtx *dgc,
                                     QEMUGLParams *params);
void sdl3_gl_destroy_context(DisplayGLCtx *dgc, QEMUGLContext ctx);
int sdl3_gl_make_context_current(DisplayGLCtx *dgc,
                                 QEMUGLContext ctx);

void sdl3_gl_scanout_disable(DisplayChangeListener *dcl);
void sdl3_gl_scanout_texture(DisplayChangeListener *dcl,
                             uint32_t backing_id,
                             bool backing_y_0_top,
                             uint32_t backing_width,
                             uint32_t backing_height,
                             uint32_t x, uint32_t y,
                             uint32_t w, uint32_t h,
                             void *d3d_tex2d);
void sdl3_gl_scanout_flush(DisplayChangeListener *dcl,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#endif /* QEMU_UI_SDL3_H */
