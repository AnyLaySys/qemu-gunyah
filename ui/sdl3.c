#include "qemu/osdep.h"
#include <SDL3/SDL_main.h>

#include "qemu/module.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/sdl3.h"
#include "system/runstate.h"
#include "system/runstate-action.h"
#include "system/system.h"
#include "qemu-main.h"

static int sdl3_num_outputs;
static struct sdl3_console *sdl3_console;

static SDL_Surface *guest_sprite_surface;
static int gui_grab;
static bool alt_grab;
static bool ctrl_grab;

static int gui_saved_grab;
static int gui_fullscreen;
static int gui_grab_code = SDL_KMOD_LALT | SDL_KMOD_LCTRL;
static SDL_Cursor *sdl_cursor_normal;
static SDL_Cursor *sdl_cursor_hidden;
static int absolute_enabled;
static bool guest_cursor;
static int guest_x, guest_y;
static SDL_Cursor *guest_sprite;
static Notifier mouse_mode_notifier;

static void sdl_update_caption(struct sdl3_console *scon);

static void sdl3_output_size(struct sdl3_console *scon, int *w, int *h)
{
    /* CurrentRenderOutputSize includes logical presentation scaling. */
    if (scon->real_renderer &&
        SDL_GetRenderOutputSize(scon->real_renderer, w, h) &&
        *w > 0 && *h > 0) {
        return;
    }
    if (scon->real_window &&
        SDL_GetWindowSizeInPixels(scon->real_window, w, h) &&
        *w > 0 && *h > 0) {
        return;
    }

    *w = surface_width(scon->surface);
    *h = surface_height(scon->surface);
}

static void sdl3_mouse_bounds(struct sdl3_console *scon, int *w, int *h)
{
    if (scon->real_renderer &&
        SDL_GetRenderLogicalPresentation(scon->real_renderer,
                                         w, h, NULL) &&
        *w > 0 && *h > 0) {
        return;
    }
    SDL_GetWindowSize(scon->real_window, w, h);
}

static uint32_t sdl3_update_refresh_rate(struct sdl3_console *scon)
{
    const SDL_DisplayMode *mode = NULL;
    SDL_DisplayID display = 0;
    uint32_t refresh_rate = 0;
    uint64_t interval;
    uint64_t precise_rate;

    if (scon->real_window) {
        display = SDL_GetDisplayForWindow(scon->real_window);
    }
    if (!display) {
        display = SDL_GetPrimaryDisplay();
    }
    if (display) {
        mode = SDL_GetCurrentDisplayMode(display);
    }
    if (mode && mode->refresh_rate_numerator > 0 &&
        mode->refresh_rate_denominator > 0) {
        precise_rate = ((uint64_t)mode->refresh_rate_numerator * 1000 +
                        mode->refresh_rate_denominator / 2) /
                       mode->refresh_rate_denominator;
        if (precise_rate > 0 && precise_rate <= UINT32_MAX) {
            refresh_rate = precise_rate;
        }
    } else if (mode && mode->refresh_rate > 0.0f &&
               mode->refresh_rate <= UINT32_MAX / 1000.0f) {
        refresh_rate = mode->refresh_rate * 1000.0f + 0.5f;
    }

    if (!refresh_rate) {
        return 0;
    }

    interval = MAX(1ULL, 1000000ULL / refresh_rate);

    if (scon->dcl.ds) {
        update_displaychangelistener(&scon->dcl, interval);
    } else {
        scon->dcl.update_interval = interval;
    }
    return refresh_rate;
}

static void sdl3_update_ui_info(struct sdl3_console *scon, bool delay)
{
    QemuUIInfo info;
    uint32_t refresh_rate;
    int width, height;

    if (!scon->surface || !qemu_console_is_graphic(scon->dcl.con)) {
        return;
    }

    refresh_rate = sdl3_update_refresh_rate(scon);

    if (!dpy_ui_info_supported(scon->dcl.con)) {
        return;
    }

    sdl3_output_size(scon, &width, &height);
    info = *dpy_get_ui_info(scon->dcl.con);
    info.width = width;
    info.height = height;
    if (refresh_rate) {
        info.refresh_rate = refresh_rate;
    }
    dpy_set_ui_info(scon->dcl.con, &info, delay);
}

#ifndef __ANDROID__
static void sdl3_window_size_for_surface(struct sdl3_console *scon,
                                         int *w, int *h)
{
    int window_w, window_h;
    int output_w, output_h;
    double scale_x = 1.0;
    double scale_y = 1.0;

    *w = surface_width(scon->surface);
    *h = surface_height(scon->surface);

    if (!scon->real_window) {
        return;
    }

    SDL_GetWindowSize(scon->real_window, &window_w, &window_h);
    sdl3_output_size(scon, &output_w, &output_h);

    if (window_w > 0 && output_w > 0) {
        scale_x = (double)output_w / window_w;
    }
    if (window_h > 0 && output_h > 0) {
        scale_y = (double)output_h / window_h;
    }

    *w = MAX(1, (int)((surface_width(scon->surface) / scale_x) + 0.5));
    *h = MAX(1, (int)((surface_height(scon->surface) / scale_y) + 0.5));
}
#endif

#ifdef __ANDROID__
static void sdl3_android_display_bounds(SDL_Rect *bounds,
                                        int fallback_w, int fallback_h)
{
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode;

    if (display && SDL_GetDisplayUsableBounds(display, bounds) &&
        bounds->w > 0 && bounds->h > 0) {
        return;
    }
    mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;
    if (mode && mode->w > 0 && mode->h > 0) {
        bounds->x = 0;
        bounds->y = 0;
        bounds->w = mode->w;
        bounds->h = mode->h;
        return;
    }
    bounds->x = 0;
    bounds->y = 0;
    bounds->w = fallback_w;
    bounds->h = fallback_h;
}
#endif

static struct sdl3_console *get_scon_from_window(uint32_t window_id)
{
    int i;
    for (i = 0; i < sdl3_num_outputs; i++) {
        if (sdl3_console[i].real_window == SDL_GetWindowFromID(window_id)) {
            return &sdl3_console[i];
        }
    }
    return NULL;
}

void sdl3_window_create(struct sdl3_console *scon)
{
    SDL_WindowFlags flags = 0;
    int window_w;
    int window_h;
#ifdef __ANDROID__
    int window_x;
    int window_y;
#endif

    if (!scon->surface) {
        return;
    }
    assert(!scon->real_window);

    window_w = surface_width(scon->surface);
    window_h = surface_height(scon->surface);

    if (gui_fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __ANDROID__
    SDL_Rect bounds;

    sdl3_android_display_bounds(&bounds, window_w, window_h);
    flags |= SDL_WINDOW_BORDERLESS;
    window_x = bounds.x;
    window_y = bounds.y;
    window_w = bounds.w;
    window_h = bounds.h;
#endif
    if (scon->hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
#ifdef CONFIG_OPENGL
    if (scon->opengl) {
        flags |= SDL_WINDOW_OPENGL;
    }
#endif

    scon->real_window = SDL_CreateWindow("", window_w, window_h, flags);
    if (!scon->real_window) {
        fprintf(stderr, "SDL: failed to create window: %s\n", SDL_GetError());
        exit(1);
    }
#ifdef __ANDROID__
    SDL_SetWindowPosition(scon->real_window, window_x, window_y);
    SDL_SetWindowSize(scon->real_window, window_w, window_h);
#endif
    if (scon->opengl) {
        scon->winctx = SDL_GL_CreateContext(scon->real_window);
        if (!scon->winctx) {
            fprintf(stderr, "SDL: failed to create GL context: %s\n",
                    SDL_GetError());
            exit(1);
        }
        SDL_GL_SetSwapInterval(0);
    } else {
        scon->real_renderer = SDL_CreateRenderer(scon->real_window, NULL);
        if (!scon->real_renderer) {
            fprintf(stderr, "SDL: failed to create renderer: %s\n",
                    SDL_GetError());
            exit(1);
        }
    }
    sdl3_update_ui_info(scon, false);
    sdl_update_caption(scon);
}

void sdl3_window_destroy(struct sdl3_console *scon)
{
    if (!scon->real_window) {
        return;
    }

    if (scon->winctx) {
        SDL_GL_DestroyContext(scon->winctx);
        scon->winctx = NULL;
    }
    if (scon->real_renderer) {
        SDL_DestroyRenderer(scon->real_renderer);
        scon->real_renderer = NULL;
    }
    SDL_DestroyWindow(scon->real_window);
    scon->real_window = NULL;
}

void sdl3_window_resize(struct sdl3_console *scon)
{
    if (!scon->real_window) {
        return;
    }

#ifdef __ANDROID__
    SDL_Rect bounds;

    sdl3_android_display_bounds(&bounds,
                                surface_width(scon->surface),
                                surface_height(scon->surface));
    SDL_SetWindowPosition(scon->real_window, bounds.x, bounds.y);
    SDL_SetWindowSize(scon->real_window, bounds.w, bounds.h);
#else
    int window_w, window_h;

    sdl3_window_size_for_surface(scon, &window_w, &window_h);
    SDL_SetWindowSize(scon->real_window, window_w, window_h);
#endif
    sdl3_update_ui_info(scon, false);
}

static void sdl3_redraw(struct sdl3_console *scon)
{
    if (scon->opengl) {
#ifdef CONFIG_OPENGL
        sdl3_gl_redraw(scon);
#endif
    } else {
        sdl3_2d_redraw(scon);
    }
}

static void sdl_update_caption(struct sdl3_console *scon)
{
    char win_title[1024];
    const char *status = "";

    if (!runstate_is_running()) {
        status = " [Stopped]";
    } else if (gui_grab) {
        if (alt_grab) {
#ifdef CONFIG_DARWIN
            status = " - Press ⌃⌥⇧G to exit grab";
#else
            status = " - Press Ctrl-Alt-Shift-G to exit grab";
#endif
        } else if (ctrl_grab) {
            status = " - Press Right-Ctrl-G to exit grab";
        } else {
#ifdef CONFIG_DARWIN
            status = " - Press ⌃⌥G to exit grab";
#else
            status = " - Press Ctrl-Alt-G to exit grab";
#endif
        }
    }

    if (qemu_name) {
        snprintf(win_title, sizeof(win_title), "QEMU (%s-%d)%s", qemu_name,
                 scon->idx, status);
    } else {
        snprintf(win_title, sizeof(win_title), "QEMU%s", status);
    }

    if (scon->real_window) {
        SDL_SetWindowTitle(scon->real_window, win_title);
    }
}

static void sdl_hide_cursor(struct sdl3_console *scon)
{
    if (scon->opts->has_show_cursor && scon->opts->show_cursor) {
        return;
    }

    SDL_HideCursor();
    SDL_SetCursor(sdl_cursor_hidden);

    if (!qemu_input_is_absolute(scon->dcl.con)) {
        SDL_SetWindowRelativeMouseMode(scon->real_window, true);
    }
}

static void sdl_show_cursor(struct sdl3_console *scon)
{
    if (scon->opts->has_show_cursor && scon->opts->show_cursor) {
        return;
    }

    if (!qemu_input_is_absolute(scon->dcl.con)) {
        SDL_SetWindowRelativeMouseMode(scon->real_window, false);
    }

    if (guest_cursor &&
        (gui_grab || qemu_input_is_absolute(scon->dcl.con) || absolute_enabled)) {
        SDL_SetCursor(guest_sprite);
    } else {
        SDL_SetCursor(sdl_cursor_normal);
    }

    SDL_ShowCursor();
}

static void sdl_grab_start(struct sdl3_console *scon)
{
    QemuConsole *con = scon ? scon->dcl.con : NULL;

    if (!con || !qemu_console_is_graphic(con)) {
        return;
    }

    if (!(SDL_GetWindowFlags(scon->real_window) & SDL_WINDOW_INPUT_FOCUS)) {
        return;
    }
    if (guest_cursor) {
        SDL_SetCursor(guest_sprite);
        if (!qemu_input_is_absolute(scon->dcl.con) && !absolute_enabled) {
            SDL_WarpMouseInWindow(scon->real_window, guest_x, guest_y);
        }
    } else {
        sdl_hide_cursor(scon);
    }
    SDL_SetWindowMouseGrab(scon->real_window, true);
    SDL_SetWindowKeyboardGrab(scon->real_window, true);
    gui_grab = 1;
    sdl_update_caption(scon);
}

static void sdl_grab_end(struct sdl3_console *scon)
{
    SDL_SetWindowKeyboardGrab(scon->real_window, false);
    SDL_SetWindowMouseGrab(scon->real_window, false);
    gui_grab = 0;
    sdl_show_cursor(scon);
    sdl_update_caption(scon);
}

static void absolute_mouse_grab(struct sdl3_console *scon)
{
    float mouse_x, mouse_y;
    int scr_w, scr_h;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    sdl3_mouse_bounds(scon, &scr_w, &scr_h);
    if (mouse_x > 0 && mouse_x < scr_w - 1 &&
        mouse_y > 0 && mouse_y < scr_h - 1) {
        sdl_grab_start(scon);
    }
}

static void sdl_mouse_mode_change(Notifier *notify, void *data)
{
    if (qemu_input_is_absolute(sdl3_console[0].dcl.con)) {
        if (!absolute_enabled) {
            absolute_enabled = 1;
            SDL_SetWindowRelativeMouseMode(sdl3_console[0].real_window, false);
            absolute_mouse_grab(&sdl3_console[0]);
        }
    } else if (absolute_enabled) {
        if (!gui_fullscreen) {
            sdl_grab_end(&sdl3_console[0]);
        }
        absolute_enabled = 0;
    }
}

static void sdl_send_mouse_event(struct sdl3_console *scon, int dx, int dy,
                                 int x, int y, int state)
{
    static uint32_t bmap[INPUT_BUTTON__MAX] = {
        [INPUT_BUTTON_LEFT]       = SDL_BUTTON_MASK(SDL_BUTTON_LEFT),
        [INPUT_BUTTON_MIDDLE]     = SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE),
        [INPUT_BUTTON_RIGHT]      = SDL_BUTTON_MASK(SDL_BUTTON_RIGHT),
        [INPUT_BUTTON_SIDE]       = SDL_BUTTON_MASK(SDL_BUTTON_X1),
        [INPUT_BUTTON_EXTRA]      = SDL_BUTTON_MASK(SDL_BUTTON_X2)
    };
    static uint32_t prev_state;

    if (prev_state != state) {
        qemu_input_update_buttons(scon->dcl.con, bmap, prev_state, state);
        prev_state = state;
    }

    if (qemu_input_is_absolute(scon->dcl.con)) {
        qemu_input_queue_abs(scon->dcl.con, INPUT_AXIS_X,
                             x, 0, surface_width(scon->surface));
        qemu_input_queue_abs(scon->dcl.con, INPUT_AXIS_Y,
                             y, 0, surface_height(scon->surface));
    } else {
        if (guest_cursor) {
            x -= guest_x;
            y -= guest_y;
            guest_x += x;
            guest_y += y;
            dx = x;
            dy = y;
        }
        qemu_input_queue_rel(scon->dcl.con, INPUT_AXIS_X, dx);
        qemu_input_queue_rel(scon->dcl.con, INPUT_AXIS_Y, dy);
    }
    qemu_input_event_sync();
}

static void toggle_full_screen(struct sdl3_console *scon)
{
    gui_fullscreen = !gui_fullscreen;
    if (gui_fullscreen) {
        SDL_SetWindowFullscreen(scon->real_window, true);
        gui_saved_grab = gui_grab;
        sdl_grab_start(scon);
    } else {
        if (!gui_saved_grab) {
            sdl_grab_end(scon);
        }
        SDL_SetWindowFullscreen(scon->real_window, false);
    }
    sdl3_update_ui_info(scon, false);
    sdl3_redraw(scon);
}

static int get_mod_state(void)
{
    SDL_Keymod mod = SDL_GetModState();

    if (alt_grab) {
        return (mod & (gui_grab_code | SDL_KMOD_LSHIFT)) ==
            (gui_grab_code | SDL_KMOD_LSHIFT);
    } else if (ctrl_grab) {
        return (mod & SDL_KMOD_RCTRL) == SDL_KMOD_RCTRL;
    } else {
        return (mod & gui_grab_code) == gui_grab_code;
    }
}

static void handle_keydown(SDL_Event *ev)
{
    int win;
    struct sdl3_console *scon = get_scon_from_window(ev->key.windowID);
    int gui_key_modifier_pressed = get_mod_state();

    if (!scon) {
        return;
    }

    scon->gui_keysym = false;

    if (!scon->ignore_hotkeys && gui_key_modifier_pressed && !ev->key.repeat) {
        switch (ev->key.scancode) {
        case SDL_SCANCODE_2:
        case SDL_SCANCODE_3:
        case SDL_SCANCODE_4:
        case SDL_SCANCODE_5:
        case SDL_SCANCODE_6:
        case SDL_SCANCODE_7:
        case SDL_SCANCODE_8:
        case SDL_SCANCODE_9:
            if (gui_grab) {
                sdl_grab_end(scon);
            }

            win = ev->key.scancode - SDL_SCANCODE_1;
            if (win < sdl3_num_outputs) {
                sdl3_console[win].hidden = !sdl3_console[win].hidden;
                if (sdl3_console[win].real_window) {
                    if (sdl3_console[win].hidden) {
                        SDL_HideWindow(sdl3_console[win].real_window);
                    } else {
                        SDL_ShowWindow(sdl3_console[win].real_window);
                    }
                }
                sdl3_release_modifiers(scon);
                scon->gui_keysym = true;
            }
            break;
        case SDL_SCANCODE_F:
            toggle_full_screen(scon);
            scon->gui_keysym = true;
            break;
        case SDL_SCANCODE_G:
            scon->gui_keysym = true;
            if (!gui_grab) {
                sdl_grab_start(scon);
            } else if (!gui_fullscreen) {
                sdl_grab_end(scon);
            }
            break;
        case SDL_SCANCODE_U:
            sdl3_window_resize(scon);
            if (!scon->opengl) {
                sdl3_2d_switch(&scon->dcl, scon->surface);
            }
            scon->gui_keysym = true;
            break;
        default:
            break;
        }
    }
    if (!scon->gui_keysym) {
        sdl3_process_key(scon, &ev->key);
    }
}

static void handle_keyup(SDL_Event *ev)
{
    struct sdl3_console *scon = get_scon_from_window(ev->key.windowID);

    if (!scon) {
        return;
    }

    scon->ignore_hotkeys = false;
    sdl3_process_key(scon, &ev->key);
}

static void handle_textinput(SDL_Event *ev)
{
    struct sdl3_console *scon = get_scon_from_window(ev->text.windowID);
    QemuConsole *con = scon ? scon->dcl.con : NULL;

    if (!con) {
        return;
    }

    if (!scon->gui_keysym && QEMU_IS_TEXT_CONSOLE(con)) {
        qemu_text_console_put_string(QEMU_TEXT_CONSOLE(con), ev->text.text, strlen(ev->text.text));
    }
}

static void handle_mousemotion(SDL_Event *ev)
{
    int max_x, max_y;
    struct sdl3_console *scon = get_scon_from_window(ev->motion.windowID);

    if (!scon || !qemu_console_is_graphic(scon->dcl.con)) {
        return;
    }
    if (scon->real_renderer) {
        SDL_ConvertEventToRenderCoordinates(scon->real_renderer, ev);
    }

    if (qemu_input_is_absolute(scon->dcl.con) || absolute_enabled) {
        int scr_w, scr_h;
        sdl3_mouse_bounds(scon, &scr_w, &scr_h);
        max_x = scr_w - 1;
        max_y = scr_h - 1;
        if (gui_grab && !gui_fullscreen
            && (ev->motion.x == 0 || ev->motion.y == 0 ||
                ev->motion.x == max_x || ev->motion.y == max_y)) {
            sdl_grab_end(scon);
        }
        if (!gui_grab &&
            (ev->motion.x > 0 && ev->motion.x < max_x &&
             ev->motion.y > 0 && ev->motion.y < max_y)) {
            sdl_grab_start(scon);
        }
    }
    if (gui_grab || qemu_input_is_absolute(scon->dcl.con) || absolute_enabled) {
        sdl_send_mouse_event(scon, (int)ev->motion.xrel,
                             (int)ev->motion.yrel,
                             (int)ev->motion.x, (int)ev->motion.y,
                             ev->motion.state);
    }
}

static void handle_mousebutton(SDL_Event *ev)
{
    int buttonstate = SDL_GetMouseState(NULL, NULL);
    SDL_MouseButtonEvent *bev;
    struct sdl3_console *scon = get_scon_from_window(ev->button.windowID);

    if (!scon || !qemu_console_is_graphic(scon->dcl.con)) {
        return;
    }
    if (scon->real_renderer) {
        SDL_ConvertEventToRenderCoordinates(scon->real_renderer, ev);
    }

    bev = &ev->button;
    if (!gui_grab && !qemu_input_is_absolute(scon->dcl.con)) {
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP &&
            bev->button == SDL_BUTTON_LEFT) {
            sdl_grab_start(scon);
        }
    } else {
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            buttonstate |= SDL_BUTTON_MASK(bev->button);
        } else {
            buttonstate &= ~SDL_BUTTON_MASK(bev->button);
        }
        sdl_send_mouse_event(scon, 0, 0, (int)bev->x, (int)bev->y,
                             buttonstate);
    }
}

static void handle_mousewheel(SDL_Event *ev)
{
    struct sdl3_console *scon = get_scon_from_window(ev->wheel.windowID);
    SDL_MouseWheelEvent *wev = &ev->wheel;
    InputButton btn;

    if (!scon || !qemu_console_is_graphic(scon->dcl.con)) {
        return;
    }

    if (wev->y > 0) {
        btn = INPUT_BUTTON_WHEEL_UP;
    } else if (wev->y < 0) {
        btn = INPUT_BUTTON_WHEEL_DOWN;
    } else if (wev->x < 0) {
        btn = INPUT_BUTTON_WHEEL_RIGHT;
    } else if (wev->x > 0) {
        btn = INPUT_BUTTON_WHEEL_LEFT;
    } else {
        return;
    }

    qemu_input_queue_btn(scon->dcl.con, btn, true);
    qemu_input_event_sync();
    qemu_input_queue_btn(scon->dcl.con, btn, false);
    qemu_input_event_sync();
}

static void handle_windowevent(SDL_Event *ev)
{
    struct sdl3_console *scon = get_scon_from_window(ev->window.windowID);
    bool allow_close = true;

    if (!scon) {
        return;
    }

    switch (ev->type) {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        sdl3_update_ui_info(scon, true);
        sdl3_redraw(scon);
        break;
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        sdl3_update_ui_info(scon, false);
        break;
    case SDL_EVENT_WINDOW_EXPOSED:
        sdl3_redraw(scon);
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
        if (!gui_grab && (qemu_input_is_absolute(scon->dcl.con) || absolute_enabled)) {
            absolute_mouse_grab(scon);
        }

        scon->ignore_hotkeys = get_mod_state();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        if (gui_grab && !gui_fullscreen) {
            sdl_grab_end(scon);
        }
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        sdl3_update_ui_info(scon, false);
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        update_displaychangelistener(&scon->dcl, 500);
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (qemu_console_is_graphic(scon->dcl.con)) {
            if (scon->opts->has_window_close && !scon->opts->window_close) {
                allow_close = false;
            }
            if (allow_close) {
                shutdown_action = SHUTDOWN_ACTION_POWEROFF;
                qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
            }
        } else {
            SDL_HideWindow(scon->real_window);
            scon->hidden = true;
        }
        break;
    case SDL_EVENT_WINDOW_SHOWN:
        scon->hidden = false;
        sdl3_update_ui_info(scon, false);
        break;
    case SDL_EVENT_WINDOW_HIDDEN:
        scon->hidden = true;
        break;
    }
}

void sdl3_poll_events(struct sdl3_console *scon)
{
    SDL_Event ev1, *ev = &ev1;
    bool allow_close = true;

    if (scon->last_vm_running != runstate_is_running()) {
        scon->last_vm_running = runstate_is_running();
        sdl_update_caption(scon);
    }

    while (SDL_PollEvent(ev)) {
        switch (ev->type) {
        case SDL_EVENT_KEY_DOWN:
            handle_keydown(ev);
            break;
        case SDL_EVENT_KEY_UP:
            handle_keyup(ev);
            break;
        case SDL_EVENT_TEXT_INPUT:
            handle_textinput(ev);
            break;
        case SDL_EVENT_QUIT:
            if (scon->opts->has_window_close && !scon->opts->window_close) {
                allow_close = false;
            }
            if (allow_close) {
                shutdown_action = SHUTDOWN_ACTION_POWEROFF;
                qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            handle_mousemotion(ev);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            handle_mousebutton(ev);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            handle_mousewheel(ev);
            break;
        default:
            if (ev->type >= SDL_EVENT_WINDOW_FIRST &&
                ev->type <= SDL_EVENT_WINDOW_LAST) {
                handle_windowevent(ev);
            }
            break;
        }
    }
}

static void sdl_mouse_warp(DisplayChangeListener *dcl,
                           int x, int y, bool on)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);

    if (!qemu_console_is_graphic(scon->dcl.con)) {
        return;
    }

    if (on) {
        if (!guest_cursor) {
            sdl_show_cursor(scon);
        }
        if (gui_grab || qemu_input_is_absolute(scon->dcl.con) || absolute_enabled) {
            SDL_SetCursor(guest_sprite);
            if (!qemu_input_is_absolute(scon->dcl.con) && !absolute_enabled) {
                SDL_WarpMouseInWindow(scon->real_window, x, y);
            }
        }
    } else if (gui_grab) {
        sdl_hide_cursor(scon);
    }
    guest_cursor = on;
    guest_x = x, guest_y = y;
}

static void sdl_mouse_define(DisplayChangeListener *dcl,
                             QEMUCursor *c)
{

    if (guest_sprite) {
        SDL_DestroyCursor(guest_sprite);
    }

    if (guest_sprite_surface) {
        SDL_DestroySurface(guest_sprite_surface);
    }

    guest_sprite_surface = SDL_CreateSurfaceFrom(c->width, c->height,
                                                  SDL_PIXELFORMAT_ARGB8888,
                                                  c->data, c->width * 4);

    if (!guest_sprite_surface) {
        fprintf(stderr, "Failed to make rgb surface from %p\n", c);
        return;
    }
    guest_sprite = SDL_CreateColorCursor(guest_sprite_surface,
                                         c->hot_x, c->hot_y);
    if (!guest_sprite) {
        fprintf(stderr, "Failed to make color cursor from %p\n", c);
        return;
    }
    if (guest_cursor &&
        (gui_grab || qemu_input_is_absolute(dcl->con) || absolute_enabled)) {
        SDL_SetCursor(guest_sprite);
    }
}

static void sdl_cleanup(void)
{
    if (guest_sprite) {
        SDL_DestroyCursor(guest_sprite);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static const DisplayChangeListenerOps dcl_2d_ops = {
    .dpy_name             = "sdl3-2d",
    .dpy_gfx_update       = sdl3_2d_update,
    .dpy_gfx_switch       = sdl3_2d_switch,
    .dpy_gfx_check_format = sdl3_2d_check_format,
    .dpy_refresh          = sdl3_2d_refresh,
    .dpy_mouse_set        = sdl_mouse_warp,
    .dpy_cursor_define    = sdl_mouse_define,
};

#ifdef CONFIG_OPENGL
static const DisplayChangeListenerOps dcl_gl_ops = {
    .dpy_name                = "sdl3-gl",
    .dpy_gfx_update          = sdl3_gl_update,
    .dpy_gfx_switch          = sdl3_gl_switch,
    .dpy_gfx_check_format    = console_gl_check_format,
    .dpy_refresh             = sdl3_gl_refresh,
    .dpy_mouse_set           = sdl_mouse_warp,
    .dpy_cursor_define       = sdl_mouse_define,

    .dpy_gl_scanout_disable  = sdl3_gl_scanout_disable,
    .dpy_gl_scanout_texture  = sdl3_gl_scanout_texture,
    .dpy_gl_update           = sdl3_gl_scanout_flush,
};

static bool
sdl3_gl_is_compatible_dcl(DisplayGLCtx *dgc,
                          DisplayChangeListener *dcl)
{
    return dcl->ops == &dcl_gl_ops;
}

static const DisplayGLCtxOps gl_ctx_ops = {
    .dpy_gl_ctx_is_compatible_dcl = sdl3_gl_is_compatible_dcl,
    .dpy_gl_ctx_create       = sdl3_gl_create_context,
    .dpy_gl_ctx_destroy      = sdl3_gl_destroy_context,
    .dpy_gl_ctx_make_current = sdl3_gl_make_context_current,
};
#endif

static void sdl3_display_early_init(DisplayOptions *o)
{
    assert(o->type == DISPLAY_TYPE_SDL);
    if (o->has_gl && o->gl) {
#ifdef CONFIG_OPENGL
        display_opengl = 1;
#endif
    }
}

static void sdl3_display_init(DisplayState *ds, DisplayOptions *o)
{
    uint8_t data = 0;
    int i;

    assert(o->type == DISPLAY_TYPE_SDL);

    if (SDL_GetHintBoolean("QEMU_ENABLE_SDL_LOGGING", false)) {
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    }

    SDL_SetMainReady();
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Could not initialize SDL(%s) - exiting\n",
                SDL_GetError());
        exit(1);
    }
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR 
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
#ifdef SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED
    SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
#endif
    SDL_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, "0");
    SDL_EnableScreenSaver();

    gui_fullscreen = o->has_full_screen && o->full_screen;

    if (o->u.sdl.has_grab_mod) {
        if (o->u.sdl.grab_mod == HOT_KEY_MOD_LSHIFT_LCTRL_LALT) {
            alt_grab = true;
        } else if (o->u.sdl.grab_mod == HOT_KEY_MOD_RCTRL) {
            ctrl_grab = true;
        }
    }

    for (i = 0;; i++) {
        QemuConsole *con = qemu_console_lookup_by_index(i);
        if (!con) {
            break;
        }
    }
    sdl3_num_outputs = i;
    if (sdl3_num_outputs == 0) {
        return;
    }
    sdl3_console = g_new0(struct sdl3_console, sdl3_num_outputs);
    for (i = 0; i < sdl3_num_outputs; i++) {
        QemuConsole *con = qemu_console_lookup_by_index(i);
        assert(con != NULL);
        if (!qemu_console_is_graphic(con) &&
            qemu_console_get_index(con) != 0) {
            sdl3_console[i].hidden = true;
        }
        sdl3_console[i].idx = i;
        sdl3_console[i].opts = o;
#ifdef CONFIG_OPENGL
        sdl3_console[i].opengl = display_opengl;
        sdl3_console[i].dcl.ops = display_opengl ? &dcl_gl_ops : &dcl_2d_ops;
        sdl3_console[i].dgc.ops = display_opengl ? &gl_ctx_ops : NULL;
#else
        sdl3_console[i].opengl = 0;
        sdl3_console[i].dcl.ops = &dcl_2d_ops;
#endif
        sdl3_console[i].dcl.con = con;
        sdl3_console[i].kbd = qkbd_state_init(con);
        if (display_opengl) {
            qemu_console_set_display_gl_ctx(con, &sdl3_console[i].dgc);
        }
        register_displaychangelistener(&sdl3_console[i].dcl);

        if (sdl3_console[i].real_window) {
            SDL_PropertiesID props =
                SDL_GetWindowProperties(sdl3_console[i].real_window);
            void *win32_window = SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
            Sint64 x11_window = SDL_GetNumberProperty(
                props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

            if (win32_window) {
                qemu_console_set_window_id(con, (uintptr_t)win32_window);
            } else if (x11_window) {
                qemu_console_set_window_id(con, x11_window);
            }
        }
    }

    mouse_mode_notifier.notify = sdl_mouse_mode_change;
    qemu_add_mouse_mode_change_notifier(&mouse_mode_notifier);

    sdl_cursor_hidden = SDL_CreateCursor(&data, &data, 8, 1, 0, 0);
    sdl_cursor_normal = SDL_GetCursor();

    if (gui_fullscreen) {
        sdl_grab_start(&sdl3_console[0]);
    }

    atexit(sdl_cleanup);

    qemu_main = NULL;
}

static QemuDisplay qemu_display_sdl3 = {
    .type       = DISPLAY_TYPE_SDL,
    .early_init = sdl3_display_early_init,
    .init       = sdl3_display_init,
};

static void register_sdl3(void)
{
    qemu_display_register(&qemu_display_sdl3);
}

type_init(register_sdl3);

#ifdef CONFIG_OPENGL
module_dep("ui-opengl");
#endif
