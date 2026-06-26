
#ifndef _VFIO_ZDEV_H_
#define _VFIO_ZDEV_H_

#include <linux/types.h>
#include <linux/vfio.h>

struct vfio_device_info_cap_zpci_base {
	struct vfio_info_cap_header header;
	__u64 start_dma;	/* Start of available DMA addresses */
	__u64 end_dma;		/* End of available DMA addresses */
	__u16 pchid;		/* Physical Channel ID */
	__u16 vfn;		/* Virtual function number */
	__u16 fmb_length;	/* Measurement Block Length (in bytes) */
	__u8 pft;		/* PCI Function Type */
	__u8 gid;		/* PCI function group ID */
	__u32 fh;		/* PCI function handle */
};

struct vfio_device_info_cap_zpci_group {
	struct vfio_info_cap_header header;
	__u64 dasm;		/* DMA Address space mask */
	__u64 msi_addr;		/* MSI address */
	__u64 flags;
#define VFIO_DEVICE_INFO_ZPCI_FLAG_REFRESH 1 /* Program-specified TLB refresh */
	__u16 mui;		/* Measurement Block Update Interval */
	__u16 noi;		/* Maximum number of MSIs */
	__u16 maxstbl;		/* Maximum Store Block Length */
	__u8 version;		/* Supported PCI Version */
	__u8 reserved;
	__u16 imaxstbl;		/* Maximum Interpreted Store Block Length */
};

struct vfio_device_info_cap_zpci_util {
	struct vfio_info_cap_header header;
	__u32 size;
	__u8 util_str[];
};

struct vfio_device_info_cap_zpci_pfip {
	struct vfio_info_cap_header header;
	__u32 size;
	__u8 pfip[];
};

#endif
