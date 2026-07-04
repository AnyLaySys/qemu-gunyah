#ifndef _LINUX_VIRTIO_NET_H
#define _LINUX_VIRTIO_NET_H
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_config.h"
#include "standard-headers/linux/virtio_types.h"
#include "standard-headers/linux/if_ether.h"

#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2 /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MTU	3	/* Initial MTU advice */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21	/* Guest can announce device on the
					 * network */
#define VIRTIO_NET_F_MQ	22	/* Device supports Receive Flow
					 * Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23	/* Set MAC address */
#define VIRTIO_NET_F_DEVICE_STATS 50	/* Device can provide device-level statistics. */
#define VIRTIO_NET_F_VQ_NOTF_COAL 52	/* Device supports virtqueue notification coalescing */
#define VIRTIO_NET_F_NOTF_COAL	53	/* Device supports notifications coalescing */
#define VIRTIO_NET_F_GUEST_USO4	54	/* Guest can handle USOv4 in. */
#define VIRTIO_NET_F_GUEST_USO6	55	/* Guest can handle USOv6 in. */
#define VIRTIO_NET_F_HOST_USO	56	/* Host can handle USO in. */
#define VIRTIO_NET_F_HASH_REPORT  57	/* Supports hash report */
#define VIRTIO_NET_F_GUEST_HDRLEN  59	/* Guest provides the exact hdr_len value. */
#define VIRTIO_NET_F_RSS	  60	/* Supports RSS RX steering */
#define VIRTIO_NET_F_RSC_EXT	  61	/* extended coalescing info */
#define VIRTIO_NET_F_STANDBY	  62	/* Act as standby for another device
					 * with the same MAC.
					 */
#define VIRTIO_NET_F_SPEED_DUPLEX 63	/* Device set linkspeed and duplex */

#ifndef VIRTIO_NET_NO_LEGACY
#define VIRTIO_NET_F_GSO	6	/* Host handles pkts w/ any GSO type */
#endif /* VIRTIO_NET_NO_LEGACY */

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_S_ANNOUNCE	2	/* Announcement is needed */

#define VIRTIO_NET_RSS_HASH_TYPE_IPv4          (1 << 0)
#define VIRTIO_NET_RSS_HASH_TYPE_TCPv4         (1 << 1)
#define VIRTIO_NET_RSS_HASH_TYPE_UDPv4         (1 << 2)
#define VIRTIO_NET_RSS_HASH_TYPE_IPv6          (1 << 3)
#define VIRTIO_NET_RSS_HASH_TYPE_TCPv6         (1 << 4)
#define VIRTIO_NET_RSS_HASH_TYPE_UDPv6         (1 << 5)
#define VIRTIO_NET_RSS_HASH_TYPE_IP_EX         (1 << 6)
#define VIRTIO_NET_RSS_HASH_TYPE_TCP_EX        (1 << 7)
#define VIRTIO_NET_RSS_HASH_TYPE_UDP_EX        (1 << 8)

struct virtio_net_config {
	uint8_t mac[ETH_ALEN];
	__virtio16 status;
	__virtio16 max_virtqueue_pairs;
	__virtio16 mtu;
	uint32_t speed;
	uint8_t duplex;
	uint8_t rss_max_key_size;
	uint16_t rss_max_indirection_table_length;
	uint32_t supported_hash_types;
} QEMU_PACKED;

struct virtio_net_hdr_v1 {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1	/* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID	2	/* Csum is valid */
#define VIRTIO_NET_HDR_F_RSC_INFO	4	/* rsc info in csum_ fields */
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4	1	/* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP		3	/* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6	4	/* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_UDP_L4	5	/* GSO frame, IPv4& IPv6 UDP (USO) */
#define VIRTIO_NET_HDR_GSO_ECN		0x80	/* TCP has ECN set */
	uint8_t gso_type;
	__virtio16 hdr_len;	/* Ethernet + IP + tcp/udp hdrs */
	__virtio16 gso_size;	/* Bytes to append to hdr_len per frame */
	union {
		struct {
			__virtio16 csum_start;
			__virtio16 csum_offset;
		};
		struct {
			__virtio16 start;
			__virtio16 offset;
		} csum;
		struct {
			uint16_t segments;
			uint16_t dup_acks;
		} rsc;
	};
	__virtio16 num_buffers;	/* Number of merged rx buffers */
};

struct virtio_net_hdr_v1_hash {
	struct virtio_net_hdr_v1 hdr;
	uint32_t hash_value;
#define VIRTIO_NET_HASH_REPORT_NONE            0
#define VIRTIO_NET_HASH_REPORT_IPv4            1
#define VIRTIO_NET_HASH_REPORT_TCPv4           2
#define VIRTIO_NET_HASH_REPORT_UDPv4           3
#define VIRTIO_NET_HASH_REPORT_IPv6            4
#define VIRTIO_NET_HASH_REPORT_TCPv6           5
#define VIRTIO_NET_HASH_REPORT_UDPv6           6
#define VIRTIO_NET_HASH_REPORT_IPv6_EX         7
#define VIRTIO_NET_HASH_REPORT_TCPv6_EX        8
#define VIRTIO_NET_HASH_REPORT_UDPv6_EX        9
	uint16_t hash_report;
	uint16_t padding;
};

#ifndef VIRTIO_NET_NO_LEGACY
struct virtio_net_hdr {
	uint8_t flags;
	uint8_t gso_type;
	__virtio16 hdr_len;		/* Ethernet + IP + tcp/udp hdrs */
	__virtio16 gso_size;		/* Bytes to append to hdr_len per frame */
	__virtio16 csum_start;	/* Position to start checksumming from */
	__virtio16 csum_offset;	/* Offset after that to place checksum */
};

struct virtio_net_hdr_mrg_rxbuf {
	struct virtio_net_hdr hdr;
	__virtio16 num_buffers;	/* Number of merged rx buffers */
};
#endif /* ...VIRTIO_NET_NO_LEGACY */

struct virtio_net_ctrl_hdr {
	uint8_t class;
	uint8_t cmd;
} QEMU_PACKED;

typedef uint8_t virtio_net_ctrl_ack;

#define VIRTIO_NET_OK     0
#define VIRTIO_NET_ERR    1

#define VIRTIO_NET_CTRL_RX    0
 #define VIRTIO_NET_CTRL_RX_PROMISC      0
 #define VIRTIO_NET_CTRL_RX_ALLMULTI     1
 #define VIRTIO_NET_CTRL_RX_ALLUNI       2
 #define VIRTIO_NET_CTRL_RX_NOMULTI      3
 #define VIRTIO_NET_CTRL_RX_NOUNI        4
 #define VIRTIO_NET_CTRL_RX_NOBCAST      5

struct virtio_net_ctrl_mac {
	__virtio32 entries;
	uint8_t macs[][ETH_ALEN];
} QEMU_PACKED;

#define VIRTIO_NET_CTRL_MAC    1
 #define VIRTIO_NET_CTRL_MAC_TABLE_SET        0
 #define VIRTIO_NET_CTRL_MAC_ADDR_SET         1

#define VIRTIO_NET_CTRL_VLAN       2
 #define VIRTIO_NET_CTRL_VLAN_ADD             0
 #define VIRTIO_NET_CTRL_VLAN_DEL             1

#define VIRTIO_NET_CTRL_ANNOUNCE       3
 #define VIRTIO_NET_CTRL_ANNOUNCE_ACK         0

#define VIRTIO_NET_CTRL_MQ   4
struct virtio_net_ctrl_mq {
	__virtio16 virtqueue_pairs;
};

 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN        1
 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX        0x8000

struct virtio_net_rss_config {
	uint32_t hash_types;
	uint16_t indirection_table_mask;
	uint16_t unclassified_queue;
	uint16_t indirection_table[1/* + indirection_table_mask */];
	uint16_t max_tx_vq;
	uint8_t hash_key_length;
	uint8_t hash_key_data[/* hash_key_length */];
};

 #define VIRTIO_NET_CTRL_MQ_RSS_CONFIG          1

struct virtio_net_hash_config {
	uint32_t hash_types;
	uint16_t reserved[4];
	uint8_t hash_key_length;
	uint8_t hash_key_data[/* hash_key_length */];
};

 #define VIRTIO_NET_CTRL_MQ_HASH_CONFIG         2

#define VIRTIO_NET_CTRL_GUEST_OFFLOADS   5
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET        0

#define VIRTIO_NET_CTRL_NOTF_COAL		6
struct virtio_net_ctrl_coal_tx {
	uint32_t tx_max_packets;
	uint32_t tx_usecs;
};

#define VIRTIO_NET_CTRL_NOTF_COAL_TX_SET		0

struct virtio_net_ctrl_coal_rx {
	uint32_t rx_max_packets;
	uint32_t rx_usecs;
};

#define VIRTIO_NET_CTRL_NOTF_COAL_RX_SET		1
#define VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET		2
#define VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET		3

struct virtio_net_ctrl_coal {
	uint32_t max_packets;
	uint32_t max_usecs;
};

struct  virtio_net_ctrl_coal_vq {
	uint16_t vqn;
	uint16_t reserved;
	struct virtio_net_ctrl_coal coal;
};

#define VIRTIO_NET_CTRL_STATS         8
#define VIRTIO_NET_CTRL_STATS_QUERY   0
#define VIRTIO_NET_CTRL_STATS_GET     1

struct virtio_net_stats_capabilities {

#define VIRTIO_NET_STATS_TYPE_CVQ       (1ULL << 32)

#define VIRTIO_NET_STATS_TYPE_RX_BASIC  (1ULL << 0)
#define VIRTIO_NET_STATS_TYPE_RX_CSUM   (1ULL << 1)
#define VIRTIO_NET_STATS_TYPE_RX_GSO    (1ULL << 2)
#define VIRTIO_NET_STATS_TYPE_RX_SPEED  (1ULL << 3)

#define VIRTIO_NET_STATS_TYPE_TX_BASIC  (1ULL << 16)
#define VIRTIO_NET_STATS_TYPE_TX_CSUM   (1ULL << 17)
#define VIRTIO_NET_STATS_TYPE_TX_GSO    (1ULL << 18)
#define VIRTIO_NET_STATS_TYPE_TX_SPEED  (1ULL << 19)

	uint64_t supported_stats_types[1];
};

struct virtio_net_ctrl_queue_stats {
	struct {
		uint16_t vq_index;
		uint16_t reserved[3];
		uint64_t types_bitmap[1];
	} stats[1];
};

struct virtio_net_stats_reply_hdr {
#define VIRTIO_NET_STATS_TYPE_REPLY_CVQ       32

#define VIRTIO_NET_STATS_TYPE_REPLY_RX_BASIC  0
#define VIRTIO_NET_STATS_TYPE_REPLY_RX_CSUM   1
#define VIRTIO_NET_STATS_TYPE_REPLY_RX_GSO    2
#define VIRTIO_NET_STATS_TYPE_REPLY_RX_SPEED  3

#define VIRTIO_NET_STATS_TYPE_REPLY_TX_BASIC  16
#define VIRTIO_NET_STATS_TYPE_REPLY_TX_CSUM   17
#define VIRTIO_NET_STATS_TYPE_REPLY_TX_GSO    18
#define VIRTIO_NET_STATS_TYPE_REPLY_TX_SPEED  19
	uint8_t type;
	uint8_t reserved;
	uint16_t vq_index;
	uint16_t reserved1;
	uint16_t size;
};

struct virtio_net_stats_cvq {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t command_num;
	uint64_t ok_num;
};

struct virtio_net_stats_rx_basic {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t rx_notifications;

	uint64_t rx_packets;
	uint64_t rx_bytes;

	uint64_t rx_interrupts;

	uint64_t rx_drops;
	uint64_t rx_drop_overruns;
};

struct virtio_net_stats_tx_basic {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t tx_notifications;

	uint64_t tx_packets;
	uint64_t tx_bytes;

	uint64_t tx_interrupts;

	uint64_t tx_drops;
	uint64_t tx_drop_malformed;
};

struct virtio_net_stats_rx_csum {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t rx_csum_valid;
	uint64_t rx_needs_csum;
	uint64_t rx_csum_none;
	uint64_t rx_csum_bad;
};

struct virtio_net_stats_tx_csum {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t tx_csum_none;
	uint64_t tx_needs_csum;
};

struct virtio_net_stats_rx_gso {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t rx_gso_packets;
	uint64_t rx_gso_bytes;
	uint64_t rx_gso_packets_coalesced;
	uint64_t rx_gso_bytes_coalesced;
};

struct virtio_net_stats_tx_gso {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t tx_gso_packets;
	uint64_t tx_gso_bytes;
	uint64_t tx_gso_segments;
	uint64_t tx_gso_segments_bytes;
	uint64_t tx_gso_packets_noseg;
	uint64_t tx_gso_bytes_noseg;
};

struct virtio_net_stats_rx_speed {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t rx_ratelimit_packets;
	uint64_t rx_ratelimit_bytes;
};

struct virtio_net_stats_tx_speed {
	struct virtio_net_stats_reply_hdr hdr;

	uint64_t tx_ratelimit_packets;
	uint64_t tx_ratelimit_bytes;
};

#endif /* _LINUX_VIRTIO_NET_H */
