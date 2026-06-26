#ifndef QEMU_PROCESSOR_H
#define QEMU_PROCESSOR_H

#if defined(__i386__) || defined(__x86_64__)
# define cpu_relax() asm volatile("rep; nop" ::: "memory")

#elif defined(__aarch64__)
# define cpu_relax() asm volatile("yield" ::: "memory")

#elif defined(__powerpc64__)
# define cpu_relax() asm volatile("or 1, 1, 1;" \
                                  "or 2, 2, 2;" ::: "memory")

#else
# define cpu_relax() barrier()
#endif

#endif /* QEMU_PROCESSOR_H */
