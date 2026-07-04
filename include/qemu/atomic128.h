
#ifndef QEMU_ATOMIC128_H
#define QEMU_ATOMIC128_H

#include "qemu/atomic.h"
#include "qemu/int128.h"

#if defined(CONFIG_ATOMIC128_OPT)
# if !defined(__OPTIMIZE__)
#  define ATTRIBUTE_ATOMIC128_OPT  __attribute__((optimize("O1")))
# endif
# define CONFIG_ATOMIC128
#endif
#ifndef ATTRIBUTE_ATOMIC128_OPT
# define ATTRIBUTE_ATOMIC128_OPT
#endif


#include "host/atomic128-cas.h.inc"
#include "host/atomic128-ldst.h.inc"

#endif /* QEMU_ATOMIC128_H */
