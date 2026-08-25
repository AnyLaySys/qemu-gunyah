#ifndef QEMU_UI_AGL_H
#define QEMU_UI_AGL_H

#ifdef __ANDROID__
#include <android/native_window.h>

/* ALS Graphics Layer: display presentation only. */
void agl_set_window(ANativeWindow *window, uint32_t refresh_rate);
bool agl_map(float x, float y, int *px, int *py, int *width, int *height);
void agl_cleanup(void);
#endif

#endif
