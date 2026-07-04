
#ifndef USER_GUEST_BASE_H
#define USER_GUEST_BASE_H

#ifndef CONFIG_USER_ONLY
#error Cannot include this header from system emulation
#endif

extern uintptr_t guest_base;

extern bool have_guest_base;

#endif
