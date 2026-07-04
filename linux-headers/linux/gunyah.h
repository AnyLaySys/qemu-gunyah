#ifndef _LINUX_GUNYAH_H
#define _LINUX_GUNYAH_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define GUNYAH_IOCTL_TYPE 'G'

#define GUNYAH_CREATE_VM _IO(GUNYAH_IOCTL_TYPE, 0x0)

enum gunyah_mem_flags {
    GUNYAH_MEM_ALLOW_READ = 1UL << 0,
    GUNYAH_MEM_ALLOW_WRITE = 1UL << 1,
    GUNYAH_MEM_ALLOW_EXEC = 1UL << 2,
    GUNYAH_MEM_ALLOW_RWX =
        GUNYAH_MEM_ALLOW_READ | GUNYAH_MEM_ALLOW_WRITE | GUNYAH_MEM_ALLOW_EXEC,
    GUNYAH_MEM_DEFAULT_ACCESS = 0x00,
    GUNYAH_MEM_FORCE_LEND = 0x10,
    GUNYAH_MEM_FORCE_SHARE = 0x20,
    GUNYAH_MEM_ACCESS_MASK = 0x70,
    GUNYAH_MEM_UNMAP = 1UL << 8,
};

struct gunyah_userspace_memory_region {
    __u32 label;
    __u32 flags;
    __u64 guest_phys_addr;
    __u64 memory_size;
    __u64 userspace_addr;
};

#define GUNYAH_VM_SET_USER_MEM_REGION _IOW(GUNYAH_IOCTL_TYPE, 0x1, \
                                           struct gunyah_userspace_memory_region)

struct gunyah_vm_dtb_config {
    __u64 guest_phys_addr;
    __u64 size;
};

#define GUNYAH_VM_SET_DTB_CONFIG _IOW(GUNYAH_IOCTL_TYPE, 0x2, \
                                      struct gunyah_vm_dtb_config)

#define GUNYAH_VM_START _IO(GUNYAH_IOCTL_TYPE, 0x3)

enum gunyah_fn_type {
    GUNYAH_FN_VCPU = 1,
    GUNYAH_FN_IRQFD,
    GUNYAH_FN_IOEVENTFD,
};

#define GUNYAH_FN_MAX_ARG_SIZE 256

struct gunyah_fn_vcpu_arg {
    __u32 id;
};

enum gunyah_irqfd_flags {
    GUNYAH_IRQFD_FLAGS_LEVEL = 1UL << 0,
};

struct gunyah_fn_irqfd_arg {
    __u32 fd;
    __u32 label;
    __u32 flags;
    __u32 padding;
};

enum gunyah_ioeventfd_flags {
    GUNYAH_IOEVENTFD_FLAGS_DATAMATCH = 1UL << 0,
};

struct gunyah_fn_ioeventfd_arg {
    __u64 datamatch;
    __u64 addr;
    __u32 len;
    __s32 fd;
    __u32 flags;
    __u32 padding;
};

struct gunyah_fn_desc {
    __u32 type;
    __u32 arg_size;
    __u64 arg;
};

#define GUNYAH_VM_ADD_FUNCTION _IOW(GUNYAH_IOCTL_TYPE, 0x4, \
                                    struct gunyah_fn_desc)
#define GUNYAH_VM_REMOVE_FUNCTION _IOW(GUNYAH_IOCTL_TYPE, 0x7, \
                                       struct gunyah_fn_desc)

enum gunyah_vm_boot_context_reg {
    GUNYAH_VM_BOOT_CONTEXT_REG_SET_X = 0,
    GUNYAH_VM_BOOT_CONTEXT_REG_SET_PC = 1,
    GUNYAH_VM_BOOT_CONTEXT_REG_SET_SP = 2,
};

#define GUNYAH_VM_BOOT_CONTEXT_REG_SHIFT 8
#define GUNYAH_VM_BOOT_CONTEXT_REG(reg, idx) \
    (((reg & 0xff) << GUNYAH_VM_BOOT_CONTEXT_REG_SHIFT) | (idx & 0xff))

struct gunyah_vm_boot_context {
    __u32 reg;
    __u32 reserved;
    __u64 value;
};

#define GUNYAH_VM_SET_BOOT_CONTEXT _IOW(GUNYAH_IOCTL_TYPE, 0xa, \
                                        struct gunyah_vm_boot_context)

enum gunyah_vm_status {
    GUNYAH_VM_STATUS_LOAD_FAILED = 1,
    GUNYAH_VM_STATUS_EXITED = 2,
    GUNYAH_VM_STATUS_CRASHED = 3,
};

#define GUNYAH_VM_MAX_EXIT_REASON_SIZE 8u

struct gunyah_vm_exit_info {
    __u16 type;
    __u16 padding;
    __u32 reason_size;
    __u8 reason[GUNYAH_VM_MAX_EXIT_REASON_SIZE];
};

enum gunyah_vcpu_exit {
    GUNYAH_VCPU_EXIT_UNKNOWN,
    GUNYAH_VCPU_EXIT_MMIO,
    GUNYAH_VCPU_EXIT_STATUS,
    GUNYAH_VCPU_EXIT_PAGE_FAULT,
};

enum gunyah_vcpu_resume_action {
    GUNYAH_VCPU_RESUME_HANDLED = 0,
    GUNYAH_VCPU_RESUME_FAULT,
    GUNYAH_VCPU_RESUME_RETRY,
};

struct gunyah_vcpu_run {
    __u8 immediate_exit;
    __u8 padding[7];
    __u32 exit_reason;

    union {
        struct {
            __u64 phys_addr;
            __u8 data[8];
            __u32 len;
            __u8 is_write;
            __u8 resume_action;
        } mmio;

        struct {
            enum gunyah_vm_status status;
            struct gunyah_vm_exit_info exit_info;
        } status;

        struct {
            __u64 phys_addr;
            __s32 attempt;
            __u8 resume_action;
        } page_fault;
    };
};

#define GUNYAH_VCPU_RUN _IO(GUNYAH_IOCTL_TYPE, 0x5)
#define GUNYAH_VCPU_MMAP_SIZE _IO(GUNYAH_IOCTL_TYPE, 0x6)

#define GUNYAH_ANDROID_IOCTL_TYPE 'A'

#define GUNYAH_VM_ANDROID_LEND_USER_MEM _IOW(GUNYAH_ANDROID_IOCTL_TYPE, 0x11, \
                                             struct gunyah_userspace_memory_region)

struct gunyah_vm_firmware_config {
    __u64 guest_phys_addr;
    __u64 size;
};

#define GUNYAH_VM_ANDROID_SET_FW_CONFIG _IOW(GUNYAH_ANDROID_IOCTL_TYPE, 0x12, \
                                             struct gunyah_vm_firmware_config)

#endif
