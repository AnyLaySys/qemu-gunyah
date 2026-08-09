

#ifndef QEMU_GUNYAH_H
#define QEMU_GUNYAH_H

#include "qemu/accel.h"
#include "qom/object.h"

#ifdef NEED_CPU_H
#include "cpu.h"
#endif

extern bool gunyah_allowed;
struct arm_boot_info;

void gunyah_set_swiotlb_size(uint64_t size);

#define gunyah_enabled() (gunyah_allowed)

#define TYPE_GUNYAH_ACCEL ACCEL_CLASS_NAME("gunyah")
typedef struct GUNYAHState GUNYAHState;
DECLARE_INSTANCE_CHECKER(GUNYAHState, GUNYAH_STATE,
                         TYPE_GUNYAH_ACCEL)

int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size);
void gunyah_arm_build_dtb(const struct arm_boot_info *binfo, void *fdt);

bool gunyah_addr_is_lend(uint64_t gpa);
void gunyah_embedded_cleanup(void);
void gunyah_arm_fdt_customize(void *fdt, uint64_t mem_base,
                uint32_t gic_phandle);

#include "qemu/event_notifier.h"
void gunyah_gic_register_irq_notifiers(EventNotifier *notifiers,
                                        int count, int base_spi);

#endif  /* QEMU_GUNYAH_H */
