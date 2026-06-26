#ifndef QEMU_UEFI_HARDWARE_INFO_H
#define QEMU_UEFI_HARDWARE_INFO_H


typedef enum {
    HardwareInfoTypeUndefined  = 0,
    HardwareInfoTypeHostBridge = 1,
    HardwareInfoQemuUefiVars   = 2,
} HARDWARE_INFO_TYPE;

typedef struct {
    union {
        uint64_t            uint64;
        HARDWARE_INFO_TYPE  value;
    } type;
    uint64_t  size;
} HARDWARE_INFO_HEADER;

typedef struct {
    uint64_t  mmio_address;
} HARDWARE_INFO_SIMPLE_DEVICE;


void hardware_info_register(HARDWARE_INFO_TYPE type, void *info, uint64_t size);

#endif /* QEMU_UEFI_HARDWARE_INFO_H */
