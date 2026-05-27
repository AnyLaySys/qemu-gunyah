/*
 * QEMU Gunyah stub
 *
 * Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#include "qemu/osdep.h"
#include "system/gunyah.h"

bool gunyah_allowed;

void gunyah_set_swiotlb_size(uint64_t size)
{
    return;
}

int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size)
{
    return -1;
}

void gunyah_arm_fdt_customize(void *fdt, uint64_t mem_base,
                uint32_t gic_phandle) {
    return;
}

void gunyah_gic_register_irq_notifiers(EventNotifier *notifiers,
                                        int count, int base_spi) {
    return;
}

bool gunyah_addr_is_lend(uint64_t gpa) {
    return false;
}
