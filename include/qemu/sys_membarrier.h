
#ifndef QEMU_SYS_MEMBARRIER_H
#define QEMU_SYS_MEMBARRIER_H

#ifdef CONFIG_MEMBARRIER
void smp_mb_global_init(void);
void smp_mb_global(void);
#define smp_mb_placeholder()       barrier()
#else
static inline void smp_mb_global_init(void) {}
#define smp_mb_global()            smp_mb()
#define smp_mb_placeholder()       smp_mb()
#endif

#endif
