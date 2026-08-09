#ifndef QEMU_UI_AGL_H
#define QEMU_UI_AGL_H

#ifdef __ANDROID__
#include <android/native_window.h>

void agl_set_native_window(ANativeWindow *window, uint32_t refresh_rate);
void agl_input_pointer(float x, float y, int buttons);
void agl_input_scroll(float x, float y);
void agl_input_key(int scan_code, bool down);
void agl_request_shutdown(void);
void agl_shutdown(void);
#endif

#endif
