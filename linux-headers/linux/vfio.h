#ifndef VFIO_H
#define VFIO_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define VFIO_API_VERSION	0




#define VFIO_TYPE1_IOMMU		1
#define VFIO_SPAPR_TCE_IOMMU		2
#define VFIO_TYPE1v2_IOMMU		3
#define VFIO_DMA_CC_IOMMU		4

#define VFIO_EEH			5

#define __VFIO_RESERVED_TYPE1_NESTING_IOMMU	6	/* Implies v2 */

#define VFIO_SPAPR_TCE_v2_IOMMU		7

#define VFIO_NOIOMMU_IOMMU		8

#define VFIO_UNMAP_ALL			9

#define VFIO_UPDATE_VADDR		10


#define VFIO_TYPE	(';')
#define VFIO_BASE	100

struct vfio_info_cap_header {
	__u16	id;		/* Identifies capability */
	__u16	version;	/* Version specific to the capability ID */
	__u32	next;		/* Offset of next capability */
};



#define VFIO_GET_API_VERSION		_IO(VFIO_TYPE, VFIO_BASE + 0)

#define VFIO_CHECK_EXTENSION		_IO(VFIO_TYPE, VFIO_BASE + 1)

#define VFIO_SET_IOMMU			_IO(VFIO_TYPE, VFIO_BASE + 2)


struct vfio_group_status {
	__u32	argsz;
	__u32	flags;
#define VFIO_GROUP_FLAGS_VIABLE		(1 << 0)
#define VFIO_GROUP_FLAGS_CONTAINER_SET	(1 << 1)
};
#define VFIO_GROUP_GET_STATUS		_IO(VFIO_TYPE, VFIO_BASE + 3)

#define VFIO_GROUP_SET_CONTAINER	_IO(VFIO_TYPE, VFIO_BASE + 4)

#define VFIO_GROUP_UNSET_CONTAINER	_IO(VFIO_TYPE, VFIO_BASE + 5)

#define VFIO_GROUP_GET_DEVICE_FD	_IO(VFIO_TYPE, VFIO_BASE + 6)


struct vfio_device_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_DEVICE_FLAGS_RESET	(1 << 0)	/* Device supports reset */
#define VFIO_DEVICE_FLAGS_PCI	(1 << 1)	/* vfio-pci device */
#define VFIO_DEVICE_FLAGS_PLATFORM (1 << 2)	/* vfio-platform device */
#define VFIO_DEVICE_FLAGS_AMBA  (1 << 3)	/* vfio-amba device */
#define VFIO_DEVICE_FLAGS_CCW	(1 << 4)	/* vfio-ccw device */
#define VFIO_DEVICE_FLAGS_AP	(1 << 5)	/* vfio-ap device */
#define VFIO_DEVICE_FLAGS_FSL_MC (1 << 6)	/* vfio-fsl-mc device */
#define VFIO_DEVICE_FLAGS_CAPS	(1 << 7)	/* Info supports caps */
#define VFIO_DEVICE_FLAGS_CDX	(1 << 8)	/* vfio-cdx device */
	__u32	num_regions;	/* Max region index + 1 */
	__u32	num_irqs;	/* Max IRQ index + 1 */
	__u32   cap_offset;	/* Offset within info struct of first cap */
	__u32   pad;
};
#define VFIO_DEVICE_GET_INFO		_IO(VFIO_TYPE, VFIO_BASE + 7)


#define VFIO_DEVICE_API_PCI_STRING		"vfio-pci"
#define VFIO_DEVICE_API_PLATFORM_STRING		"vfio-platform"
#define VFIO_DEVICE_API_AMBA_STRING		"vfio-amba"
#define VFIO_DEVICE_API_CCW_STRING		"vfio-ccw"
#define VFIO_DEVICE_API_AP_STRING		"vfio-ap"

#define VFIO_DEVICE_INFO_CAP_ZPCI_BASE		1
#define VFIO_DEVICE_INFO_CAP_ZPCI_GROUP		2
#define VFIO_DEVICE_INFO_CAP_ZPCI_UTIL		3
#define VFIO_DEVICE_INFO_CAP_ZPCI_PFIP		4

#define VFIO_DEVICE_INFO_CAP_PCI_ATOMIC_COMP	5
struct vfio_device_info_cap_pci_atomic_comp {
	struct vfio_info_cap_header header;
	__u32 flags;
#define VFIO_PCI_ATOMIC_COMP32	(1 << 0)
#define VFIO_PCI_ATOMIC_COMP64	(1 << 1)
#define VFIO_PCI_ATOMIC_COMP128	(1 << 2)
	__u32 reserved;
};

struct vfio_region_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_REGION_INFO_FLAG_READ	(1 << 0) /* Region supports read */
#define VFIO_REGION_INFO_FLAG_WRITE	(1 << 1) /* Region supports write */
#define VFIO_REGION_INFO_FLAG_MMAP	(1 << 2) /* Region supports mmap */
#define VFIO_REGION_INFO_FLAG_CAPS	(1 << 3) /* Info supports caps */
	__u32	index;		/* Region index */
	__u32	cap_offset;	/* Offset within info struct of first cap */
	__aligned_u64	size;	/* Region size (bytes) */
	__aligned_u64	offset;	/* Region offset from start of device fd */
};
#define VFIO_DEVICE_GET_REGION_INFO	_IO(VFIO_TYPE, VFIO_BASE + 8)

#define VFIO_REGION_INFO_CAP_SPARSE_MMAP	1

struct vfio_region_sparse_mmap_area {
	__aligned_u64	offset;	/* Offset of mmap'able area within region */
	__aligned_u64	size;	/* Size of mmap'able area */
};

struct vfio_region_info_cap_sparse_mmap {
	struct vfio_info_cap_header header;
	__u32	nr_areas;
	__u32	reserved;
	struct vfio_region_sparse_mmap_area areas[];
};

#define VFIO_REGION_INFO_CAP_TYPE	2

struct vfio_region_info_cap_type {
	struct vfio_info_cap_header header;
	__u32 type;	/* global per bus driver */
	__u32 subtype;	/* type specific */
};


#define VFIO_REGION_TYPE_PCI_VENDOR_TYPE	(1 << 31)
#define VFIO_REGION_TYPE_PCI_VENDOR_MASK	(0xffff)
#define VFIO_REGION_TYPE_GFX                    (1)
#define VFIO_REGION_TYPE_CCW			(2)
#define VFIO_REGION_TYPE_MIGRATION_DEPRECATED   (3)


#define VFIO_REGION_SUBTYPE_INTEL_IGD_OPREGION	(1)
#define VFIO_REGION_SUBTYPE_INTEL_IGD_HOST_CFG	(2)
#define VFIO_REGION_SUBTYPE_INTEL_IGD_LPC_CFG	(3)

#define VFIO_REGION_SUBTYPE_NVIDIA_NVLINK2_RAM	(1)

#define VFIO_REGION_SUBTYPE_IBM_NVLINK2_ATSD	(1)

#define VFIO_REGION_SUBTYPE_GFX_EDID            (1)

struct vfio_region_gfx_edid {
	__u32 edid_offset;
	__u32 edid_max_size;
	__u32 edid_size;
	__u32 max_xres;
	__u32 max_yres;
	__u32 link_state;
#define VFIO_DEVICE_GFX_LINK_STATE_UP    1
#define VFIO_DEVICE_GFX_LINK_STATE_DOWN  2
};

#define VFIO_REGION_SUBTYPE_CCW_ASYNC_CMD	(1)
#define VFIO_REGION_SUBTYPE_CCW_SCHIB		(2)
#define VFIO_REGION_SUBTYPE_CCW_CRW		(3)

#define VFIO_REGION_SUBTYPE_MIGRATION_DEPRECATED (1)

struct vfio_device_migration_info {
	__u32 device_state;         /* VFIO device state */
#define VFIO_DEVICE_STATE_V1_STOP      (0)
#define VFIO_DEVICE_STATE_V1_RUNNING   (1 << 0)
#define VFIO_DEVICE_STATE_V1_SAVING    (1 << 1)
#define VFIO_DEVICE_STATE_V1_RESUMING  (1 << 2)
#define VFIO_DEVICE_STATE_MASK      (VFIO_DEVICE_STATE_V1_RUNNING | \
				     VFIO_DEVICE_STATE_V1_SAVING |  \
				     VFIO_DEVICE_STATE_V1_RESUMING)

#define VFIO_DEVICE_STATE_VALID(state) \
	(state & VFIO_DEVICE_STATE_V1_RESUMING ? \
	(state & VFIO_DEVICE_STATE_MASK) == VFIO_DEVICE_STATE_V1_RESUMING : 1)

#define VFIO_DEVICE_STATE_IS_ERROR(state) \
	((state & VFIO_DEVICE_STATE_MASK) == (VFIO_DEVICE_STATE_V1_SAVING | \
					      VFIO_DEVICE_STATE_V1_RESUMING))

#define VFIO_DEVICE_STATE_SET_ERROR(state) \
	((state & ~VFIO_DEVICE_STATE_MASK) | VFIO_DEVICE_STATE_V1_SAVING | \
					     VFIO_DEVICE_STATE_V1_RESUMING)

	__u32 reserved;
	__aligned_u64 pending_bytes;
	__aligned_u64 data_offset;
	__aligned_u64 data_size;
};

#define VFIO_REGION_INFO_CAP_MSIX_MAPPABLE	3

#define VFIO_REGION_INFO_CAP_NVLINK2_SSATGT	4

struct vfio_region_info_cap_nvlink2_ssatgt {
	struct vfio_info_cap_header header;
	__aligned_u64 tgt;
};

#define VFIO_REGION_INFO_CAP_NVLINK2_LNKSPD	5

struct vfio_region_info_cap_nvlink2_lnkspd {
	struct vfio_info_cap_header header;
	__u32 link_speed;
	__u32 __pad;
};

struct vfio_irq_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_IRQ_INFO_EVENTFD		(1 << 0)
#define VFIO_IRQ_INFO_MASKABLE		(1 << 1)
#define VFIO_IRQ_INFO_AUTOMASKED	(1 << 2)
#define VFIO_IRQ_INFO_NORESIZE		(1 << 3)
	__u32	index;		/* IRQ index */
	__u32	count;		/* Number of IRQs within this index */
};
#define VFIO_DEVICE_GET_IRQ_INFO	_IO(VFIO_TYPE, VFIO_BASE + 9)

struct vfio_irq_set {
	__u32	argsz;
	__u32	flags;
#define VFIO_IRQ_SET_DATA_NONE		(1 << 0) /* Data not present */
#define VFIO_IRQ_SET_DATA_BOOL		(1 << 1) /* Data is bool (u8) */
#define VFIO_IRQ_SET_DATA_EVENTFD	(1 << 2) /* Data is eventfd (s32) */
#define VFIO_IRQ_SET_ACTION_MASK	(1 << 3) /* Mask interrupt */
#define VFIO_IRQ_SET_ACTION_UNMASK	(1 << 4) /* Unmask interrupt */
#define VFIO_IRQ_SET_ACTION_TRIGGER	(1 << 5) /* Trigger interrupt */
	__u32	index;
	__u32	start;
	__u32	count;
	__u8	data[];
};
#define VFIO_DEVICE_SET_IRQS		_IO(VFIO_TYPE, VFIO_BASE + 10)

#define VFIO_IRQ_SET_DATA_TYPE_MASK	(VFIO_IRQ_SET_DATA_NONE | \
					 VFIO_IRQ_SET_DATA_BOOL | \
					 VFIO_IRQ_SET_DATA_EVENTFD)
#define VFIO_IRQ_SET_ACTION_TYPE_MASK	(VFIO_IRQ_SET_ACTION_MASK | \
					 VFIO_IRQ_SET_ACTION_UNMASK | \
					 VFIO_IRQ_SET_ACTION_TRIGGER)
#define VFIO_DEVICE_RESET		_IO(VFIO_TYPE, VFIO_BASE + 11)


enum {
	VFIO_PCI_BAR0_REGION_INDEX,
	VFIO_PCI_BAR1_REGION_INDEX,
	VFIO_PCI_BAR2_REGION_INDEX,
	VFIO_PCI_BAR3_REGION_INDEX,
	VFIO_PCI_BAR4_REGION_INDEX,
	VFIO_PCI_BAR5_REGION_INDEX,
	VFIO_PCI_ROM_REGION_INDEX,
	VFIO_PCI_CONFIG_REGION_INDEX,
	VFIO_PCI_VGA_REGION_INDEX,
	VFIO_PCI_NUM_REGIONS = 9 /* Fixed user ABI, region indexes >=9 use */
};

enum {
	VFIO_PCI_INTX_IRQ_INDEX,
	VFIO_PCI_MSI_IRQ_INDEX,
	VFIO_PCI_MSIX_IRQ_INDEX,
	VFIO_PCI_ERR_IRQ_INDEX,
	VFIO_PCI_REQ_IRQ_INDEX,
	VFIO_PCI_NUM_IRQS
};


enum {
	VFIO_CCW_CONFIG_REGION_INDEX,
	VFIO_CCW_NUM_REGIONS
};

enum {
	VFIO_CCW_IO_IRQ_INDEX,
	VFIO_CCW_CRW_IRQ_INDEX,
	VFIO_CCW_REQ_IRQ_INDEX,
	VFIO_CCW_NUM_IRQS
};

enum {
	VFIO_AP_REQ_IRQ_INDEX,
	VFIO_AP_NUM_IRQS
};

struct vfio_pci_dependent_device {
	union {
		__u32   group_id;
		__u32	devid;
#define VFIO_PCI_DEVID_OWNED		0
#define VFIO_PCI_DEVID_NOT_OWNED	-1
	};
	__u16	segment;
	__u8	bus;
	__u8	devfn; /* Use PCI_SLOT/PCI_FUNC */
};

struct vfio_pci_hot_reset_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_PCI_HOT_RESET_FLAG_DEV_ID		(1 << 0)
#define VFIO_PCI_HOT_RESET_FLAG_DEV_ID_OWNED	(1 << 1)
	__u32	count;
	struct vfio_pci_dependent_device	devices[];
};

#define VFIO_DEVICE_GET_PCI_HOT_RESET_INFO	_IO(VFIO_TYPE, VFIO_BASE + 12)

struct vfio_pci_hot_reset {
	__u32	argsz;
	__u32	flags;
	__u32	count;
	__s32	group_fds[];
};

#define VFIO_DEVICE_PCI_HOT_RESET	_IO(VFIO_TYPE, VFIO_BASE + 13)

struct vfio_device_gfx_plane_info {
	__u32 argsz;
	__u32 flags;
#define VFIO_GFX_PLANE_TYPE_PROBE (1 << 0)
#define VFIO_GFX_PLANE_TYPE_DMABUF (1 << 1)
#define VFIO_GFX_PLANE_TYPE_REGION (1 << 2)
	__u32 drm_plane_type;	/* type of plane: DRM_PLANE_TYPE_* */
	__u32 drm_format;	/* drm format of plane */
	__aligned_u64 drm_format_mod;   /* tiled mode */
	__u32 width;	/* width of plane */
	__u32 height;	/* height of plane */
	__u32 stride;	/* stride of plane */
	__u32 size;	/* size of plane in bytes, align on page*/
	__u32 x_pos;	/* horizontal position of cursor plane */
	__u32 y_pos;	/* vertical position of cursor plane*/
	__u32 x_hot;    /* horizontal position of cursor hotspot */
	__u32 y_hot;    /* vertical position of cursor hotspot */
	union {
		__u32 region_index;	/* region index */
		__u32 dmabuf_id;	/* dma-buf id */
	};
	__u32 reserved;
};

#define VFIO_DEVICE_QUERY_GFX_PLANE _IO(VFIO_TYPE, VFIO_BASE + 14)


#define VFIO_DEVICE_GET_GFX_DMABUF _IO(VFIO_TYPE, VFIO_BASE + 15)

struct vfio_device_ioeventfd {
	__u32	argsz;
	__u32	flags;
#define VFIO_DEVICE_IOEVENTFD_8		(1 << 0) /* 1-byte write */
#define VFIO_DEVICE_IOEVENTFD_16	(1 << 1) /* 2-byte write */
#define VFIO_DEVICE_IOEVENTFD_32	(1 << 2) /* 4-byte write */
#define VFIO_DEVICE_IOEVENTFD_64	(1 << 3) /* 8-byte write */
#define VFIO_DEVICE_IOEVENTFD_SIZE_MASK	(0xf)
	__aligned_u64	offset;		/* device fd offset of write */
	__aligned_u64	data;		/* data to be written */
	__s32	fd;			/* -1 for de-assignment */
	__u32	reserved;
};

#define VFIO_DEVICE_IOEVENTFD		_IO(VFIO_TYPE, VFIO_BASE + 16)

struct vfio_device_feature {
	__u32	argsz;
	__u32	flags;
#define VFIO_DEVICE_FEATURE_MASK	(0xffff) /* 16-bit feature index */
#define VFIO_DEVICE_FEATURE_GET		(1 << 16) /* Get feature into data[] */
#define VFIO_DEVICE_FEATURE_SET		(1 << 17) /* Set feature from data[] */
#define VFIO_DEVICE_FEATURE_PROBE	(1 << 18) /* Probe feature support */
	__u8	data[];
};

#define VFIO_DEVICE_FEATURE		_IO(VFIO_TYPE, VFIO_BASE + 17)

struct vfio_device_bind_iommufd {
	__u32		argsz;
	__u32		flags;
	__s32		iommufd;
	__u32		out_devid;
};

#define VFIO_DEVICE_BIND_IOMMUFD	_IO(VFIO_TYPE, VFIO_BASE + 18)

struct vfio_device_attach_iommufd_pt {
	__u32	argsz;
	__u32	flags;
	__u32	pt_id;
};

#define VFIO_DEVICE_ATTACH_IOMMUFD_PT		_IO(VFIO_TYPE, VFIO_BASE + 19)

struct vfio_device_detach_iommufd_pt {
	__u32	argsz;
	__u32	flags;
};

#define VFIO_DEVICE_DETACH_IOMMUFD_PT		_IO(VFIO_TYPE, VFIO_BASE + 20)

#define VFIO_DEVICE_FEATURE_PCI_VF_TOKEN	(0)

struct vfio_device_feature_migration {
	__aligned_u64 flags;
#define VFIO_MIGRATION_STOP_COPY	(1 << 0)
#define VFIO_MIGRATION_P2P		(1 << 1)
#define VFIO_MIGRATION_PRE_COPY		(1 << 2)
};
#define VFIO_DEVICE_FEATURE_MIGRATION 1

struct vfio_device_feature_mig_state {
	__u32 device_state; /* From enum vfio_device_mig_state */
	__s32 data_fd;
};
#define VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE 2

enum vfio_device_mig_state {
	VFIO_DEVICE_STATE_ERROR = 0,
	VFIO_DEVICE_STATE_STOP = 1,
	VFIO_DEVICE_STATE_RUNNING = 2,
	VFIO_DEVICE_STATE_STOP_COPY = 3,
	VFIO_DEVICE_STATE_RESUMING = 4,
	VFIO_DEVICE_STATE_RUNNING_P2P = 5,
	VFIO_DEVICE_STATE_PRE_COPY = 6,
	VFIO_DEVICE_STATE_PRE_COPY_P2P = 7,
	VFIO_DEVICE_STATE_NR,
};

struct vfio_precopy_info {
	__u32 argsz;
	__u32 flags;
	__aligned_u64 initial_bytes;
	__aligned_u64 dirty_bytes;
};

#define VFIO_MIG_GET_PRECOPY_INFO _IO(VFIO_TYPE, VFIO_BASE + 21)

#define VFIO_DEVICE_FEATURE_LOW_POWER_ENTRY 3

struct vfio_device_low_power_entry_with_wakeup {
	__s32 wakeup_eventfd;
	__u32 reserved;
};

#define VFIO_DEVICE_FEATURE_LOW_POWER_ENTRY_WITH_WAKEUP 4

#define VFIO_DEVICE_FEATURE_LOW_POWER_EXIT 5

struct vfio_device_feature_dma_logging_control {
	__aligned_u64 page_size;
	__u32 num_ranges;
	__u32 __reserved;
	__aligned_u64 ranges;
};

struct vfio_device_feature_dma_logging_range {
	__aligned_u64 iova;
	__aligned_u64 length;
};

#define VFIO_DEVICE_FEATURE_DMA_LOGGING_START 6

#define VFIO_DEVICE_FEATURE_DMA_LOGGING_STOP 7

struct vfio_device_feature_dma_logging_report {
	__aligned_u64 iova;
	__aligned_u64 length;
	__aligned_u64 page_size;
	__aligned_u64 bitmap;
};

#define VFIO_DEVICE_FEATURE_DMA_LOGGING_REPORT 8


struct vfio_device_feature_mig_data_size {
	__aligned_u64 stop_copy_length;
};

#define VFIO_DEVICE_FEATURE_MIG_DATA_SIZE 9

struct vfio_device_feature_bus_master {
	__u32 op;
#define		VFIO_DEVICE_FEATURE_CLEAR_MASTER	0	/* Clear Bus Master */
#define		VFIO_DEVICE_FEATURE_SET_MASTER		1	/* Set Bus Master */
};
#define VFIO_DEVICE_FEATURE_BUS_MASTER 10


struct vfio_iommu_type1_info {
	__u32	argsz;
	__u32	flags;
#define VFIO_IOMMU_INFO_PGSIZES (1 << 0)	/* supported page sizes info */
#define VFIO_IOMMU_INFO_CAPS	(1 << 1)	/* Info supports caps */
	__aligned_u64	iova_pgsizes;		/* Bitmap of supported page sizes */
	__u32   cap_offset;	/* Offset within info struct of first cap */
	__u32   pad;
};

#define VFIO_IOMMU_TYPE1_INFO_CAP_IOVA_RANGE  1

struct vfio_iova_range {
	__u64	start;
	__u64	end;
};

struct vfio_iommu_type1_info_cap_iova_range {
	struct	vfio_info_cap_header header;
	__u32	nr_iovas;
	__u32	reserved;
	struct	vfio_iova_range iova_ranges[];
};

#define VFIO_IOMMU_TYPE1_INFO_CAP_MIGRATION  2

struct vfio_iommu_type1_info_cap_migration {
	struct	vfio_info_cap_header header;
	__u32	flags;
	__u64	pgsize_bitmap;
	__u64	max_dirty_bitmap_size;		/* in bytes */
};

#define VFIO_IOMMU_TYPE1_INFO_DMA_AVAIL 3

struct vfio_iommu_type1_info_dma_avail {
	struct	vfio_info_cap_header header;
	__u32	avail;
};

#define VFIO_IOMMU_GET_INFO _IO(VFIO_TYPE, VFIO_BASE + 12)

struct vfio_iommu_type1_dma_map {
	__u32	argsz;
	__u32	flags;
#define VFIO_DMA_MAP_FLAG_READ (1 << 0)		/* readable from device */
#define VFIO_DMA_MAP_FLAG_WRITE (1 << 1)	/* writable from device */
#define VFIO_DMA_MAP_FLAG_VADDR (1 << 2)
	__u64	vaddr;				/* Process virtual address */
	__u64	iova;				/* IO virtual address */
	__u64	size;				/* Size of mapping (bytes) */
};

#define VFIO_IOMMU_MAP_DMA _IO(VFIO_TYPE, VFIO_BASE + 13)

struct vfio_bitmap {
	__u64        pgsize;	/* page size for bitmap in bytes */
	__u64        size;	/* in bytes */
	__u64 *data;	/* one bit per page */
};

struct vfio_iommu_type1_dma_unmap {
	__u32	argsz;
	__u32	flags;
#define VFIO_DMA_UNMAP_FLAG_GET_DIRTY_BITMAP (1 << 0)
#define VFIO_DMA_UNMAP_FLAG_ALL		     (1 << 1)
#define VFIO_DMA_UNMAP_FLAG_VADDR	     (1 << 2)
	__u64	iova;				/* IO virtual address */
	__u64	size;				/* Size of mapping (bytes) */
	__u8    data[];
};

#define VFIO_IOMMU_UNMAP_DMA _IO(VFIO_TYPE, VFIO_BASE + 14)

#define VFIO_IOMMU_ENABLE	_IO(VFIO_TYPE, VFIO_BASE + 15)
#define VFIO_IOMMU_DISABLE	_IO(VFIO_TYPE, VFIO_BASE + 16)

struct vfio_iommu_type1_dirty_bitmap {
	__u32        argsz;
	__u32        flags;
#define VFIO_IOMMU_DIRTY_PAGES_FLAG_START	(1 << 0)
#define VFIO_IOMMU_DIRTY_PAGES_FLAG_STOP	(1 << 1)
#define VFIO_IOMMU_DIRTY_PAGES_FLAG_GET_BITMAP	(1 << 2)
	__u8         data[];
};

struct vfio_iommu_type1_dirty_bitmap_get {
	__u64              iova;	/* IO virtual address */
	__u64              size;	/* Size of iova range */
	struct vfio_bitmap bitmap;
};

#define VFIO_IOMMU_DIRTY_PAGES             _IO(VFIO_TYPE, VFIO_BASE + 17)


struct vfio_iommu_spapr_tce_ddw_info {
	__u64 pgsizes;			/* Bitmap of supported page sizes */
	__u32 max_dynamic_windows_supported;
	__u32 levels;
};

struct vfio_iommu_spapr_tce_info {
	__u32 argsz;
	__u32 flags;
#define VFIO_IOMMU_SPAPR_INFO_DDW	(1 << 0)	/* DDW supported */
	__u32 dma32_window_start;	/* 32 bit window start (bytes) */
	__u32 dma32_window_size;	/* 32 bit window size (bytes) */
	struct vfio_iommu_spapr_tce_ddw_info ddw;
};

#define VFIO_IOMMU_SPAPR_TCE_GET_INFO	_IO(VFIO_TYPE, VFIO_BASE + 12)

struct vfio_eeh_pe_err {
	__u32 type;
	__u32 func;
	__u64 addr;
	__u64 mask;
};

struct vfio_eeh_pe_op {
	__u32 argsz;
	__u32 flags;
	__u32 op;
	union {
		struct vfio_eeh_pe_err err;
	};
};

#define VFIO_EEH_PE_DISABLE		0	/* Disable EEH functionality */
#define VFIO_EEH_PE_ENABLE		1	/* Enable EEH functionality  */
#define VFIO_EEH_PE_UNFREEZE_IO		2	/* Enable IO for frozen PE   */
#define VFIO_EEH_PE_UNFREEZE_DMA	3	/* Enable DMA for frozen PE  */
#define VFIO_EEH_PE_GET_STATE		4	/* PE state retrieval        */
#define  VFIO_EEH_PE_STATE_NORMAL	0	/* PE in functional state    */
#define  VFIO_EEH_PE_STATE_RESET	1	/* PE reset in progress      */
#define  VFIO_EEH_PE_STATE_STOPPED	2	/* Stopped DMA and IO        */
#define  VFIO_EEH_PE_STATE_STOPPED_DMA	4	/* Stopped DMA only          */
#define  VFIO_EEH_PE_STATE_UNAVAIL	5	/* State unavailable         */
#define VFIO_EEH_PE_RESET_DEACTIVATE	5	/* Deassert PE reset         */
#define VFIO_EEH_PE_RESET_HOT		6	/* Assert hot reset          */
#define VFIO_EEH_PE_RESET_FUNDAMENTAL	7	/* Assert fundamental reset  */
#define VFIO_EEH_PE_CONFIGURE		8	/* PE configuration          */
#define VFIO_EEH_PE_INJECT_ERR		9	/* Inject EEH error          */

#define VFIO_EEH_PE_OP			_IO(VFIO_TYPE, VFIO_BASE + 21)

struct vfio_iommu_spapr_register_memory {
	__u32	argsz;
	__u32	flags;
	__u64	vaddr;				/* Process virtual address */
	__u64	size;				/* Size of mapping (bytes) */
};
#define VFIO_IOMMU_SPAPR_REGISTER_MEMORY	_IO(VFIO_TYPE, VFIO_BASE + 17)

#define VFIO_IOMMU_SPAPR_UNREGISTER_MEMORY	_IO(VFIO_TYPE, VFIO_BASE + 18)

struct vfio_iommu_spapr_tce_create {
	__u32 argsz;
	__u32 flags;
	__u32 page_shift;
	__u32 __resv1;
	__u64 window_size;
	__u32 levels;
	__u32 __resv2;
	__u64 start_addr;
};
#define VFIO_IOMMU_SPAPR_TCE_CREATE	_IO(VFIO_TYPE, VFIO_BASE + 19)

struct vfio_iommu_spapr_tce_remove {
	__u32 argsz;
	__u32 flags;
	__u64 start_addr;
};
#define VFIO_IOMMU_SPAPR_TCE_REMOVE	_IO(VFIO_TYPE, VFIO_BASE + 20)


#endif /* VFIO_H */
