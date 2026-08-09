
#ifndef _LINUX_GUNYAH_H
#define _LINUX_GUNYAH_H


#include <linux/types.h>
#include <linux/ioctl.h>

#define GH_IOCTL_TYPE			'G'

#define GH_CREATE_VM			_IO(GH_IOCTL_TYPE, 0x0) /* Returns a Gunyah VM fd */


enum gh_mem_flags {
	GH_MEM_ALLOW_READ	= 1UL << 0,
	GH_MEM_ALLOW_WRITE	= 1UL << 1,
	GH_MEM_ALLOW_EXEC	= 1UL << 2,
};

struct gh_userspace_memory_region {
	__u32 label;
	__u32 flags;
	__u64 guest_phys_addr;
	__u64 memory_size;
	__u64 userspace_addr;
};

#define GH_VM_SET_USER_MEM_REGION	_IOW(GH_IOCTL_TYPE, 0x1, \
						struct gh_userspace_memory_region)

struct gh_vm_dtb_config {
	__u64 guest_phys_addr;
	__u64 size;
};
#define GH_VM_SET_DTB_CONFIG	_IOW(GH_IOCTL_TYPE, 0x2, struct gh_vm_dtb_config)

#define GH_VM_START		_IO(GH_IOCTL_TYPE, 0x3)

struct gh_vm_boot_context {
	__u32 reg;
	__u32 reserved;
	__u64 value;
};

#define GH_VM_BOOT_CONTEXT_REG_SHIFT	8
#define GH_VM_BOOT_CONTEXT_REG_SET_X	0
#define GH_VM_BOOT_CONTEXT_REG_SET_PC	1
#define GH_VM_BOOT_CONTEXT_REG_SET_SP	2

#define GH_VM_SET_BOOT_CONTEXT	_IOW(GH_IOCTL_TYPE, 0xa, struct gh_vm_boot_context)

enum gh_fn_type {
	GH_FN_VCPU = 1,
	GH_FN_IRQFD,
	GH_FN_IOEVENTFD,
};

#define GH_FN_MAX_ARG_SIZE		256

struct gh_fn_vcpu_arg {
	__u32 id;
};

enum gh_irqfd_flags {
	GH_IRQFD_FLAGS_LEVEL		= 1UL << 0,
};

struct gh_fn_irqfd_arg {
	__u32 fd;
	__u32 label;
	__u32 flags;
	__u32 padding;
};

enum gh_ioeventfd_flags {
	GH_IOEVENTFD_FLAGS_DATAMATCH	= 1UL << 0,
};

struct gh_fn_ioeventfd_arg {
	__u64 datamatch;
	__u64 addr;        /* legal mmio address */
	__u32 len;         /* 1, 2, 4, or 8 bytes; or 0 to ignore length */
	__s32 fd;
	__u32 flags;
	__u32 padding;
};

struct gh_fn_desc {
	__u32 type;
	__u32 arg_size;
	__u64 arg;
};

#define GH_VM_ADD_FUNCTION	_IOW(GH_IOCTL_TYPE, 0x4, struct gh_fn_desc)
#define GH_VM_REMOVE_FUNCTION	_IOW(GH_IOCTL_TYPE, 0x7, struct gh_fn_desc)


enum gh_vm_status {
	GH_VM_STATUS_LOAD_FAILED	= 1,
	GH_VM_STATUS_EXITED		= 2,
	GH_VM_STATUS_CRASHED		= 3,
};

#define GH_VM_MAX_EXIT_REASON_SIZE	8u

struct gh_vm_exit_info {
	__u16 type;
	__u16 padding;
	__u32 reason_size;
	__u8 reason[GH_VM_MAX_EXIT_REASON_SIZE];
};

enum gh_vcpu_exit {
	GH_VCPU_EXIT_UNKNOWN,
	GH_VCPU_EXIT_MMIO,
	GH_VCPU_EXIT_STATUS,
	GH_VCPU_EXIT_PAGE_FAULT,
};

enum gh_vcpu_resume_action {
	GH_VCPU_RESUME_HANDLED = 0,
	GH_VCPU_RESUME_FAULT,
	GH_VCPU_RESUME_RETRY,
};

struct gh_vcpu_run {
	__u8 immediate_exit;
	__u8 padding[7];

	__u32 exit_reason;

	union {
		struct {
			__u64 phys_addr;
			__u8  data[8];
			__u32 len;
			__u8  is_write;
			__u8  resume_action;
		} mmio;

		struct {
			enum gh_vm_status status;
			struct gh_vm_exit_info exit_info;
		} status;

		struct {
			__u64 phys_addr;
			__s32 attempt;
			__u8  resume_action;
		} page_fault;
	};
};

#define GH_VCPU_RUN		_IO(GH_IOCTL_TYPE, 0x5)
#define GH_VCPU_MMAP_SIZE	_IO(GH_IOCTL_TYPE, 0x6)

#define GH_ANDROID_IOCTL_TYPE		'A'

#define GH_VM_ANDROID_LEND_USER_MEM	_IOW(GH_ANDROID_IOCTL_TYPE, 0x11, \
						struct gh_userspace_memory_region)

struct gh_vm_firmware_config {
	__u64 guest_phys_addr;
	__u64 size;
};

#define GH_VM_ANDROID_SET_FW_CONFIG	_IOW(GH_ANDROID_IOCTL_TYPE, 0x12, \
						struct gh_vm_firmware_config)

#endif
