
#ifndef QEMU_CPUID_H
#define QEMU_CPUID_H

#ifndef CONFIG_CPUID_H
# error "<cpuid.h> is unusable with this compiler"
#endif

#include <cpuid.h>


#ifndef bit_CMOV
#define bit_CMOV        (1 << 15)
#endif
#ifndef bit_SSE2
#define bit_SSE2        (1 << 26)
#endif

#ifndef bit_PCLMUL
#define bit_PCLMUL      (1 << 1)
#endif
#ifndef bit_SSE4_1
#define bit_SSE4_1      (1 << 19)
#endif
#ifndef bit_MOVBE
#define bit_MOVBE       (1 << 22)
#endif
#ifndef bit_OSXSAVE
#define bit_OSXSAVE     (1 << 27)
#endif
#ifndef bit_AVX
#define bit_AVX         (1 << 28)
#endif

#ifndef bit_BMI
#define bit_BMI         (1 << 3)
#endif
#ifndef bit_AVX2
#define bit_AVX2        (1 << 5)
#endif
#ifndef bit_BMI2
#define bit_BMI2        (1 << 8)
#endif
#ifndef bit_AVX512F
#define bit_AVX512F     (1 << 16)
#endif
#ifndef bit_AVX512DQ
#define bit_AVX512DQ    (1 << 17)
#endif
#ifndef bit_AVX512BW
#define bit_AVX512BW    (1 << 30)
#endif
#ifndef bit_AVX512VL
#define bit_AVX512VL    (1u << 31)
#endif

#ifndef bit_AVX512VBMI2
#define bit_AVX512VBMI2 (1 << 6)
#endif

#ifndef bit_LZCNT
#define bit_LZCNT       (1 << 5)
#endif


#ifndef signature_INTEL_ecx
#define signature_INTEL_ebx     0x756e6547
#define signature_INTEL_edx     0x49656e69
#define signature_INTEL_ecx     0x6c65746e
#endif

#ifndef signature_AMD_ecx
#define signature_AMD_ebx       0x68747541
#define signature_AMD_edx       0x69746e65
#define signature_AMD_ecx       0x444d4163
#endif

static inline unsigned xgetbv_low(unsigned c)
{
    unsigned a, d;
    asm("xgetbv" : "=a"(a), "=d"(d) : "c"(c));
    return a;
}

#endif /* QEMU_CPUID_H */
