
#ifndef HW_INTC_IOAPIC_H
#define HW_INTC_IOAPIC_H

#define IOAPIC_NUM_PINS 24
#define IO_APIC_DEFAULT_ADDRESS 0xfec00000
#define IO_APIC_SECONDARY_ADDRESS (IO_APIC_DEFAULT_ADDRESS + 0x10000)
#define IO_APIC_SECONDARY_IRQBASE 24 /* primary 0 -> 23, secondary 24 -> 47 */

#define TYPE_KVM_IOAPIC "kvm-ioapic"
#define TYPE_IOAPIC "ioapic"

void ioapic_eoi_broadcast(int vector);

#endif /* HW_INTC_IOAPIC_H */
