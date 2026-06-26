
#ifndef QEMU_ARM_BSA_H
#define QEMU_ARM_BSA_H

#define ARCH_TIMER_S_EL2_VIRT_IRQ  19
#define ARCH_TIMER_S_EL2_IRQ       20
#define VIRTUAL_PMU_IRQ            23
#define ARCH_GIC_MAINT_IRQ         25
#define ARCH_TIMER_NS_EL2_IRQ      26
#define ARCH_TIMER_VIRT_IRQ        27
#define ARCH_TIMER_NS_EL2_VIRT_IRQ 28
#define ARCH_TIMER_S_EL1_IRQ       29
#define ARCH_TIMER_NS_EL1_IRQ      30

#define INTID_TO_PPI(irq) ((irq) - 16)

#endif /* QEMU_ARM_BSA_H */
