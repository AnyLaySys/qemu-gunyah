

#ifndef QEMU_WHPX_H
#define QEMU_WHPX_H

#ifdef COMPILING_PER_TARGET

#ifdef CONFIG_WHPX

int whpx_enabled(void);
bool whpx_apic_in_platform(void);

#else /* CONFIG_WHPX */

#define whpx_enabled() (0)
#define whpx_apic_in_platform() (0)

#endif /* CONFIG_WHPX */

#endif /* COMPILING_PER_TARGET */

#endif /* QEMU_WHPX_H */
