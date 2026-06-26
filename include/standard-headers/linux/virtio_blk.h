#ifndef _LINUX_VIRTIO_BLK_H
#define _LINUX_VIRTIO_BLK_H
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"
#include "standard-headers/linux/virtio_types.h"

#define VIRTIO_BLK_F_SIZE_MAX	1	/* Indicates maximum segment size */
#define VIRTIO_BLK_F_SEG_MAX	2	/* Indicates maximum # of segments */
#define VIRTIO_BLK_F_GEOMETRY	4	/* Legacy geometry available  */
#define VIRTIO_BLK_F_RO		5	/* Disk is read-only */
#define VIRTIO_BLK_F_BLK_SIZE	6	/* Block size of disk is available*/
#define VIRTIO_BLK_F_TOPOLOGY	10	/* Topology information is available */
#define VIRTIO_BLK_F_MQ		12	/* support more than one vq */
#define VIRTIO_BLK_F_DISCARD	13	/* DISCARD is supported */
#define VIRTIO_BLK_F_WRITE_ZEROES	14	/* WRITE ZEROES is supported */
#define VIRTIO_BLK_F_SECURE_ERASE	16 /* Secure Erase is supported */
#define VIRTIO_BLK_F_ZONED		17	/* Zoned block device */

#ifndef VIRTIO_BLK_NO_LEGACY
#define VIRTIO_BLK_F_BARRIER	0	/* Does host support barriers? */
#define VIRTIO_BLK_F_SCSI	7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_FLUSH	9	/* Flush command supported */
#define VIRTIO_BLK_F_CONFIG_WCE	11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_WCE VIRTIO_BLK_F_FLUSH
#endif /* !VIRTIO_BLK_NO_LEGACY */

#define VIRTIO_BLK_ID_BYTES	20	/* ID string length */

struct virtio_blk_config {
	__virtio64 capacity;
	__virtio32 size_max;
	__virtio32 seg_max;
	struct virtio_blk_geometry {
		__virtio16 cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;

	__virtio32 blk_size;

	uint8_t physical_block_exp;
	uint8_t alignment_offset;
	__virtio16 min_io_size;
	__virtio32 opt_io_size;

	uint8_t wce;
	uint8_t unused;

	__virtio16 num_queues;

	__virtio32 max_discard_sectors;
	__virtio32 max_discard_seg;
	__virtio32 discard_sector_alignment;

	__virtio32 max_write_zeroes_sectors;
	__virtio32 max_write_zeroes_seg;
	uint8_t write_zeroes_may_unmap;

	uint8_t unused1[3];

	__virtio32 max_secure_erase_sectors;
	__virtio32 max_secure_erase_seg;
	__virtio32 secure_erase_sector_alignment;

	struct virtio_blk_zoned_characteristics {
		__virtio32 zone_sectors;
		__virtio32 max_open_zones;
		__virtio32 max_active_zones;
		__virtio32 max_append_sectors;
		__virtio32 write_granularity;
		uint8_t model;
		uint8_t unused2[3];
	} zoned;
} QEMU_PACKED;


#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1

#ifndef VIRTIO_BLK_NO_LEGACY
#define VIRTIO_BLK_T_SCSI_CMD	2
#endif /* VIRTIO_BLK_NO_LEGACY */

#define VIRTIO_BLK_T_FLUSH	4

#define VIRTIO_BLK_T_GET_ID    8

#define VIRTIO_BLK_T_DISCARD	11

#define VIRTIO_BLK_T_WRITE_ZEROES	13

#define VIRTIO_BLK_T_SECURE_ERASE	14

#define VIRTIO_BLK_T_ZONE_APPEND    15

#define VIRTIO_BLK_T_ZONE_REPORT    16

#define VIRTIO_BLK_T_ZONE_OPEN      18

#define VIRTIO_BLK_T_ZONE_CLOSE     20

#define VIRTIO_BLK_T_ZONE_FINISH    22

#define VIRTIO_BLK_T_ZONE_RESET     24

#define VIRTIO_BLK_T_ZONE_RESET_ALL 26

#ifndef VIRTIO_BLK_NO_LEGACY
#define VIRTIO_BLK_T_BARRIER	0x80000000
#endif /* !VIRTIO_BLK_NO_LEGACY */

struct virtio_blk_outhdr {
	__virtio32 type;
	__virtio32 ioprio;
	__virtio64 sector;
};


#define VIRTIO_BLK_Z_NONE      0
#define VIRTIO_BLK_Z_HM        1
#define VIRTIO_BLK_Z_HA        2

struct virtio_blk_zone_descriptor {
	__virtio64 z_cap;
	__virtio64 z_start;
	__virtio64 z_wp;
	uint8_t z_type;
	uint8_t z_state;
	uint8_t reserved[38];
};

struct virtio_blk_zone_report {
	__virtio64 nr_zones;
	uint8_t reserved[56];
	struct virtio_blk_zone_descriptor zones[];
};


#define VIRTIO_BLK_ZT_CONV         1
#define VIRTIO_BLK_ZT_SWR          2
#define VIRTIO_BLK_ZT_SWP          3


#define VIRTIO_BLK_ZS_NOT_WP       0
#define VIRTIO_BLK_ZS_EMPTY        1
#define VIRTIO_BLK_ZS_IOPEN        2
#define VIRTIO_BLK_ZS_EOPEN        3
#define VIRTIO_BLK_ZS_CLOSED       4
#define VIRTIO_BLK_ZS_RDONLY       13
#define VIRTIO_BLK_ZS_FULL         14
#define VIRTIO_BLK_ZS_OFFLINE      15

#define VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP	0x00000001

struct virtio_blk_discard_write_zeroes {
	uint64_t sector;
	uint32_t num_sectors;
	uint32_t flags;
};

#ifndef VIRTIO_BLK_NO_LEGACY
struct virtio_scsi_inhdr {
	__virtio32 errors;
	__virtio32 data_len;
	__virtio32 sense_len;
	__virtio32 residual;
};
#endif /* !VIRTIO_BLK_NO_LEGACY */

#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1
#define VIRTIO_BLK_S_UNSUPP	2

#define VIRTIO_BLK_S_ZONE_INVALID_CMD     3
#define VIRTIO_BLK_S_ZONE_UNALIGNED_WP    4
#define VIRTIO_BLK_S_ZONE_OPEN_RESOURCE   5
#define VIRTIO_BLK_S_ZONE_ACTIVE_RESOURCE 6

#endif /* _LINUX_VIRTIO_BLK_H */
