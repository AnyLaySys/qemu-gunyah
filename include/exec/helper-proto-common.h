
#ifndef HELPER_PROTO_COMMON_H
#define HELPER_PROTO_COMMON_H

#include "qemu/atomic128.h"  /* for HAVE_CMPXCHG128 */

#define HELPER_H "accel/tcg/tcg-runtime.h"
#include "exec/helper-proto.h.inc"
#undef  HELPER_H

#endif /* HELPER_PROTO_COMMON_H */
