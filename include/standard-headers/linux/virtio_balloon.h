#ifndef _LINUX_VIRTIO_BALLOON_H
#define _LINUX_VIRTIO_BALLOON_H
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"

#define VIRTIO_BALLOON_F_MUST_TELL_HOST	0 /* Tell before reclaiming pages */
#define VIRTIO_BALLOON_F_STATS_VQ	1 /* Memory Stats virtqueue */
#define VIRTIO_BALLOON_F_DEFLATE_ON_OOM	2 /* Deflate balloon on OOM */
#define VIRTIO_BALLOON_F_FREE_PAGE_HINT	3 /* VQ to report free pages */
#define VIRTIO_BALLOON_F_PAGE_POISON	4 /* Guest is using page poisoning */
#define VIRTIO_BALLOON_F_REPORTING	5 /* Page reporting virtqueue */

#define VIRTIO_BALLOON_PFN_SHIFT 12

#define VIRTIO_BALLOON_CMD_ID_STOP	0
#define VIRTIO_BALLOON_CMD_ID_DONE	1
struct virtio_balloon_config {
	uint32_t num_pages;
	uint32_t actual;
	union {
		uint32_t free_page_hint_cmd_id;
		uint32_t free_page_report_cmd_id;	/* deprecated */
	};
	uint32_t poison_val;
};

#define VIRTIO_BALLOON_S_SWAP_IN  0   /* Amount of memory swapped in */
#define VIRTIO_BALLOON_S_SWAP_OUT 1   /* Amount of memory swapped out */
#define VIRTIO_BALLOON_S_MAJFLT   2   /* Number of major faults */
#define VIRTIO_BALLOON_S_MINFLT   3   /* Number of minor faults */
#define VIRTIO_BALLOON_S_MEMFREE  4   /* Total amount of free memory */
#define VIRTIO_BALLOON_S_MEMTOT   5   /* Total amount of memory */
#define VIRTIO_BALLOON_S_AVAIL    6   /* Available memory as in /proc */
#define VIRTIO_BALLOON_S_CACHES   7   /* Disk caches */
#define VIRTIO_BALLOON_S_HTLB_PGALLOC  8  /* Hugetlb page allocations */
#define VIRTIO_BALLOON_S_HTLB_PGFAIL   9  /* Hugetlb page allocation failures */
#define VIRTIO_BALLOON_S_OOM_KILL      10 /* OOM killer invocations */
#define VIRTIO_BALLOON_S_ALLOC_STALL   11 /* Stall count of memory allocatoin */
#define VIRTIO_BALLOON_S_ASYNC_SCAN    12 /* Amount of memory scanned asynchronously */
#define VIRTIO_BALLOON_S_DIRECT_SCAN   13 /* Amount of memory scanned directly */
#define VIRTIO_BALLOON_S_ASYNC_RECLAIM 14 /* Amount of memory reclaimed asynchronously */
#define VIRTIO_BALLOON_S_DIRECT_RECLAIM 15 /* Amount of memory reclaimed directly */
#define VIRTIO_BALLOON_S_NR       16

#define VIRTIO_BALLOON_S_NAMES_WITH_PREFIX(VIRTIO_BALLOON_S_NAMES_prefix) { \
	VIRTIO_BALLOON_S_NAMES_prefix "swap-in", \
	VIRTIO_BALLOON_S_NAMES_prefix "swap-out", \
	VIRTIO_BALLOON_S_NAMES_prefix "major-faults", \
	VIRTIO_BALLOON_S_NAMES_prefix "minor-faults", \
	VIRTIO_BALLOON_S_NAMES_prefix "free-memory", \
	VIRTIO_BALLOON_S_NAMES_prefix "total-memory", \
	VIRTIO_BALLOON_S_NAMES_prefix "available-memory", \
	VIRTIO_BALLOON_S_NAMES_prefix "disk-caches", \
	VIRTIO_BALLOON_S_NAMES_prefix "hugetlb-allocations", \
	VIRTIO_BALLOON_S_NAMES_prefix "hugetlb-failures", \
	VIRTIO_BALLOON_S_NAMES_prefix "oom-kills", \
	VIRTIO_BALLOON_S_NAMES_prefix "alloc-stalls", \
	VIRTIO_BALLOON_S_NAMES_prefix "async-scans", \
	VIRTIO_BALLOON_S_NAMES_prefix "direct-scans", \
	VIRTIO_BALLOON_S_NAMES_prefix "async-reclaims", \
	VIRTIO_BALLOON_S_NAMES_prefix "direct-reclaims" \
}

#define VIRTIO_BALLOON_S_NAMES VIRTIO_BALLOON_S_NAMES_WITH_PREFIX("")

struct virtio_balloon_stat {
	__virtio16 tag;
	__virtio64 val;
} QEMU_PACKED;

#endif /* _LINUX_VIRTIO_BALLOON_H */
