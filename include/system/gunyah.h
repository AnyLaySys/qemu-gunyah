/*
 * QEMU Gunyah hypervisor support
 *
 * Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* header to be included in non-Gunyah-specific code */

#ifndef QEMU_GUNYAH_H
#define QEMU_GUNYAH_H

#include "qemu/accel.h"
#include "qom/object.h"

#ifdef NEED_CPU_H
#include "cpu.h"
#endif

extern bool gunyah_allowed;

void gunyah_set_swiotlb_size(uint64_t size);

#define gunyah_enabled() (gunyah_allowed)

#define TYPE_GUNYAH_ACCEL ACCEL_CLASS_NAME("gunyah")
typedef struct GUNYAHState GUNYAHState;
DECLARE_INSTANCE_CHECKER(GUNYAHState, GUNYAH_STATE,
                         TYPE_GUNYAH_ACCEL)

int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size);

/*
 * Check if a guest physical address falls in LEND'd (host-inaccessible)
 * memory.  Returns true if the address is in the LEND region, meaning
 * the host process will SIGBUS if it tries to access the corresponding
 * HVA.  Used by virtio-gpu and other devices to avoid mapping LEND'd
 * memory directly.
 */
bool gunyah_addr_is_lend(uint64_t gpa);
void gunyah_arm_fdt_customize(void *fdt, uint64_t mem_base,
                uint32_t gic_phandle);

/* SimpleFB: framebuffer in SHARE'd memory for Gunyah display */
uint64_t gunyah_get_simplefb_addr(void);
uint64_t gunyah_get_simplefb_size(void);

#include "qemu/event_notifier.h"
void gunyah_gic_register_irq_notifiers(EventNotifier *notifiers,
                                        int count, int base_spi);

#endif  /* QEMU_GUNYAH_H */
