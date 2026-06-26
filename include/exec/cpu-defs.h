#ifndef CPU_DEFS_H
#define CPU_DEFS_H

#ifndef COMPILING_PER_TARGET
#error cpu.h included from common code
#endif

#include "cpu-param.h"

#ifndef TARGET_LONG_BITS
# error TARGET_LONG_BITS must be defined in cpu-param.h
#endif
#ifndef TARGET_PHYS_ADDR_SPACE_BITS
# error TARGET_PHYS_ADDR_SPACE_BITS must be defined in cpu-param.h
#endif
#ifndef TARGET_VIRT_ADDR_SPACE_BITS
# error TARGET_VIRT_ADDR_SPACE_BITS must be defined in cpu-param.h
#endif
#ifndef TARGET_PAGE_BITS
# ifdef TARGET_PAGE_BITS_VARY
#  ifndef TARGET_PAGE_BITS_MIN
#   error TARGET_PAGE_BITS_MIN must be defined in cpu-param.h
#  endif
# else
#  error TARGET_PAGE_BITS must be defined in cpu-param.h
# endif
#endif

#include "exec/target_long.h"

#endif
