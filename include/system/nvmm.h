

#ifndef QEMU_NVMM_H
#define QEMU_NVMM_H

#ifdef COMPILING_PER_TARGET

#ifdef CONFIG_NVMM

int nvmm_enabled(void);

#else /* CONFIG_NVMM */

#define nvmm_enabled() (0)

#endif /* CONFIG_NVMM */

#endif /* COMPILING_PER_TARGET */

#endif /* QEMU_NVMM_H */
