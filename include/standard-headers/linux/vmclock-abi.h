

#ifndef __VMCLOCK_ABI_H__
#define __VMCLOCK_ABI_H__

#include "standard-headers/linux/types.h"

struct vmclock_abi {
	uint32_t magic;
#define VMCLOCK_MAGIC	0x4b4c4356 /* "VCLK" */
	uint32_t size;		/* Size of region containing this structure */
	uint16_t version;	/* 1 */
	uint8_t counter_id; /* Matches VIRTIO_RTC_COUNTER_xxx except INVALID */
#define VMCLOCK_COUNTER_ARM_VCNT	0
#define VMCLOCK_COUNTER_X86_TSC		1
#define VMCLOCK_COUNTER_INVALID		0xff
	uint8_t time_type; /* Matches VIRTIO_RTC_TYPE_xxx */
#define VMCLOCK_TIME_UTC			0	/* Since 1970-01-01 00:00:00z */
#define VMCLOCK_TIME_TAI			1	/* Since 1970-01-01 00:00:00z */
#define VMCLOCK_TIME_MONOTONIC			2	/* Since undefined epoch */
#define VMCLOCK_TIME_INVALID_SMEARED		3	/* Not supported */
#define VMCLOCK_TIME_INVALID_MAYBE_SMEARED	4	/* Not supported */

	uint32_t seq_count;	/* Low bit means an update is in progress */
	uint64_t disruption_marker;
	uint64_t flags;
#define VMCLOCK_FLAG_TAI_OFFSET_VALID		(1 << 0)
#define VMCLOCK_FLAG_DISRUPTION_SOON		(1 << 1) /* About a day */
#define VMCLOCK_FLAG_DISRUPTION_IMMINENT	(1 << 2) /* About an hour */
#define VMCLOCK_FLAG_PERIOD_ESTERROR_VALID	(1 << 3)
#define VMCLOCK_FLAG_PERIOD_MAXERROR_VALID	(1 << 4)
#define VMCLOCK_FLAG_TIME_ESTERROR_VALID	(1 << 5)
#define VMCLOCK_FLAG_TIME_MAXERROR_VALID	(1 << 6)
#define VMCLOCK_FLAG_TIME_MONOTONIC		(1 << 7)

	uint8_t pad[2];
	uint8_t clock_status;
#define VMCLOCK_STATUS_UNKNOWN		0
#define VMCLOCK_STATUS_INITIALIZING	1
#define VMCLOCK_STATUS_SYNCHRONIZED	2
#define VMCLOCK_STATUS_FREERUNNING	3
#define VMCLOCK_STATUS_UNRELIABLE	4

	uint8_t leap_second_smearing_hint; /* Matches VIRTIO_RTC_SUBTYPE_xxx */
#define VMCLOCK_SMEARING_STRICT		0
#define VMCLOCK_SMEARING_NOON_LINEAR	1
#define VMCLOCK_SMEARING_UTC_SLS	2
	uint16_t tai_offset_sec; /* Actually two's complement signed */
	uint8_t leap_indicator;
#define VMCLOCK_LEAP_NONE	0x00	/* No known nearby leap second */
#define VMCLOCK_LEAP_PRE_POS	0x01	/* Positive leap second at EOM */
#define VMCLOCK_LEAP_PRE_NEG	0x02	/* Negative leap second at EOM */
#define VMCLOCK_LEAP_POS	0x03	/* Set during 23:59:60 second */
#define VMCLOCK_LEAP_POST_POS	0x04
#define VMCLOCK_LEAP_POST_NEG	0x05

	uint8_t counter_period_shift;
	uint64_t counter_value;
	uint64_t counter_period_frac_sec;
	uint64_t counter_period_esterror_rate_frac_sec;
	uint64_t counter_period_maxerror_rate_frac_sec;

	uint64_t time_sec;		/* Seconds since time_type epoch */
	uint64_t time_frac_sec;		/* Units of 1/2^64 of a second */
	uint64_t time_esterror_nanosec;
	uint64_t time_maxerror_nanosec;
};

#endif /*  __VMCLOCK_ABI_H__ */
