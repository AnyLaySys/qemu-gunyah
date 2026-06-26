
#ifndef _LINUX_ETHTOOL_H
#define _LINUX_ETHTOOL_H

#include "net/eth.h"

#include "standard-headers/linux/const.h"
#include "standard-headers/linux/types.h"
#include "standard-headers/linux/if_ether.h"

#include <limits.h> /* for INT_MAX */



struct ethtool_cmd {
	uint32_t	cmd;
	uint32_t	supported;
	uint32_t	advertising;
	uint16_t	speed;
	uint8_t	duplex;
	uint8_t	port;
	uint8_t	phy_address;
	uint8_t	transceiver;
	uint8_t	autoneg;
	uint8_t	mdio_support;
	uint32_t	maxtxpkt;
	uint32_t	maxrxpkt;
	uint16_t	speed_hi;
	uint8_t	eth_tp_mdix;
	uint8_t	eth_tp_mdix_ctrl;
	uint32_t	lp_advertising;
	uint32_t	reserved[2];
};

static inline void ethtool_cmd_speed_set(struct ethtool_cmd *ep,
					 uint32_t speed)
{
	ep->speed = (uint16_t)(speed & 0xFFFF);
	ep->speed_hi = (uint16_t)(speed >> 16);
}

static inline uint32_t ethtool_cmd_speed(const struct ethtool_cmd *ep)
{
	return (ep->speed_hi << 16) | ep->speed;
}

#define ETH_MDIO_SUPPORTS_C22	1

#define ETH_MDIO_SUPPORTS_C45	2

#define ETHTOOL_FWVERS_LEN	32
#define ETHTOOL_BUSINFO_LEN	32
#define ETHTOOL_EROMVERS_LEN	32

struct ethtool_drvinfo {
	uint32_t	cmd;
	char	driver[32];
	char	version[32];
	char	fw_version[ETHTOOL_FWVERS_LEN];
	char	bus_info[ETHTOOL_BUSINFO_LEN];
	char	erom_version[ETHTOOL_EROMVERS_LEN];
	char	reserved2[12];
	uint32_t	n_priv_flags;
	uint32_t	n_stats;
	uint32_t	testinfo_len;
	uint32_t	eedump_len;
	uint32_t	regdump_len;
};

#define SOPASS_MAX	6

struct ethtool_wolinfo {
	uint32_t	cmd;
	uint32_t	supported;
	uint32_t	wolopts;
	uint8_t	sopass[SOPASS_MAX];
};

struct ethtool_value {
	uint32_t	cmd;
	uint32_t	data;
};

#define PFC_STORM_PREVENTION_AUTO	0xffff
#define PFC_STORM_PREVENTION_DISABLE	0

enum tunable_id {
	ETHTOOL_ID_UNSPEC,
	ETHTOOL_RX_COPYBREAK,
	ETHTOOL_TX_COPYBREAK,
	ETHTOOL_PFC_PREVENTION_TOUT, /* timeout in msecs */
	ETHTOOL_TX_COPYBREAK_BUF_SIZE,
	__ETHTOOL_TUNABLE_COUNT,
};

enum tunable_type_id {
	ETHTOOL_TUNABLE_UNSPEC,
	ETHTOOL_TUNABLE_U8,
	ETHTOOL_TUNABLE_U16,
	ETHTOOL_TUNABLE_U32,
	ETHTOOL_TUNABLE_U64,
	ETHTOOL_TUNABLE_STRING,
	ETHTOOL_TUNABLE_S8,
	ETHTOOL_TUNABLE_S16,
	ETHTOOL_TUNABLE_S32,
	ETHTOOL_TUNABLE_S64,
};

struct ethtool_tunable {
	uint32_t	cmd;
	uint32_t	id;
	uint32_t	type_id;
	uint32_t	len;
	void	*data[];
};

#define DOWNSHIFT_DEV_DEFAULT_COUNT	0xff
#define DOWNSHIFT_DEV_DISABLE		0

#define ETHTOOL_PHY_FAST_LINK_DOWN_ON	0
#define ETHTOOL_PHY_FAST_LINK_DOWN_OFF	0xff

#define ETHTOOL_PHY_EDPD_DFLT_TX_MSECS		0xffff
#define ETHTOOL_PHY_EDPD_NO_TX			0xfffe
#define ETHTOOL_PHY_EDPD_DISABLE		0

enum phy_tunable_id {
	ETHTOOL_PHY_ID_UNSPEC,
	ETHTOOL_PHY_DOWNSHIFT,
	ETHTOOL_PHY_FAST_LINK_DOWN,
	ETHTOOL_PHY_EDPD,
	__ETHTOOL_PHY_TUNABLE_COUNT,
};

struct ethtool_regs {
	uint32_t	cmd;
	uint32_t	version;
	uint32_t	len;
	uint8_t	data[];
};

struct ethtool_eeprom {
	uint32_t	cmd;
	uint32_t	magic;
	uint32_t	offset;
	uint32_t	len;
	uint8_t	data[];
};

struct ethtool_eee {
	uint32_t	cmd;
	uint32_t	supported;
	uint32_t	advertised;
	uint32_t	lp_advertised;
	uint32_t	eee_active;
	uint32_t	eee_enabled;
	uint32_t	tx_lpi_enabled;
	uint32_t	tx_lpi_timer;
	uint32_t	reserved[2];
};

struct ethtool_modinfo {
	uint32_t   cmd;
	uint32_t   type;
	uint32_t   eeprom_len;
	uint32_t   reserved[8];
};

struct ethtool_coalesce {
	uint32_t	cmd;
	uint32_t	rx_coalesce_usecs;
	uint32_t	rx_max_coalesced_frames;
	uint32_t	rx_coalesce_usecs_irq;
	uint32_t	rx_max_coalesced_frames_irq;
	uint32_t	tx_coalesce_usecs;
	uint32_t	tx_max_coalesced_frames;
	uint32_t	tx_coalesce_usecs_irq;
	uint32_t	tx_max_coalesced_frames_irq;
	uint32_t	stats_block_coalesce_usecs;
	uint32_t	use_adaptive_rx_coalesce;
	uint32_t	use_adaptive_tx_coalesce;
	uint32_t	pkt_rate_low;
	uint32_t	rx_coalesce_usecs_low;
	uint32_t	rx_max_coalesced_frames_low;
	uint32_t	tx_coalesce_usecs_low;
	uint32_t	tx_max_coalesced_frames_low;
	uint32_t	pkt_rate_high;
	uint32_t	rx_coalesce_usecs_high;
	uint32_t	rx_max_coalesced_frames_high;
	uint32_t	tx_coalesce_usecs_high;
	uint32_t	tx_max_coalesced_frames_high;
	uint32_t	rate_sample_interval;
};

struct ethtool_ringparam {
	uint32_t	cmd;
	uint32_t	rx_max_pending;
	uint32_t	rx_mini_max_pending;
	uint32_t	rx_jumbo_max_pending;
	uint32_t	tx_max_pending;
	uint32_t	rx_pending;
	uint32_t	rx_mini_pending;
	uint32_t	rx_jumbo_pending;
	uint32_t	tx_pending;
};


struct ethtool_channels {
	uint32_t	cmd;
	uint32_t	max_rx;
	uint32_t	max_tx;
	uint32_t	max_other;
	uint32_t	max_combined;
	uint32_t	rx_count;
	uint32_t	tx_count;
	uint32_t	other_count;
	uint32_t	combined_count;
};

struct ethtool_pauseparam {
	uint32_t	cmd;
	uint32_t	autoneg;
	uint32_t	rx_pause;
	uint32_t	tx_pause;
};

enum ethtool_link_ext_state {
	ETHTOOL_LINK_EXT_STATE_AUTONEG,
	ETHTOOL_LINK_EXT_STATE_LINK_TRAINING_FAILURE,
	ETHTOOL_LINK_EXT_STATE_LINK_LOGICAL_MISMATCH,
	ETHTOOL_LINK_EXT_STATE_BAD_SIGNAL_INTEGRITY,
	ETHTOOL_LINK_EXT_STATE_NO_CABLE,
	ETHTOOL_LINK_EXT_STATE_CABLE_ISSUE,
	ETHTOOL_LINK_EXT_STATE_EEPROM_ISSUE,
	ETHTOOL_LINK_EXT_STATE_CALIBRATION_FAILURE,
	ETHTOOL_LINK_EXT_STATE_POWER_BUDGET_EXCEEDED,
	ETHTOOL_LINK_EXT_STATE_OVERHEAT,
	ETHTOOL_LINK_EXT_STATE_MODULE,
};

enum ethtool_link_ext_substate_autoneg {
	ETHTOOL_LINK_EXT_SUBSTATE_AN_NO_PARTNER_DETECTED = 1,
	ETHTOOL_LINK_EXT_SUBSTATE_AN_ACK_NOT_RECEIVED,
	ETHTOOL_LINK_EXT_SUBSTATE_AN_NEXT_PAGE_EXCHANGE_FAILED,
	ETHTOOL_LINK_EXT_SUBSTATE_AN_NO_PARTNER_DETECTED_FORCE_MODE,
	ETHTOOL_LINK_EXT_SUBSTATE_AN_FEC_MISMATCH_DURING_OVERRIDE,
	ETHTOOL_LINK_EXT_SUBSTATE_AN_NO_HCD,
};

enum ethtool_link_ext_substate_link_training {
	ETHTOOL_LINK_EXT_SUBSTATE_LT_KR_FRAME_LOCK_NOT_ACQUIRED = 1,
	ETHTOOL_LINK_EXT_SUBSTATE_LT_KR_LINK_INHIBIT_TIMEOUT,
	ETHTOOL_LINK_EXT_SUBSTATE_LT_KR_LINK_PARTNER_DID_NOT_SET_RECEIVER_READY,
	ETHTOOL_LINK_EXT_SUBSTATE_LT_REMOTE_FAULT,
};

enum ethtool_link_ext_substate_link_logical_mismatch {
	ETHTOOL_LINK_EXT_SUBSTATE_LLM_PCS_DID_NOT_ACQUIRE_BLOCK_LOCK = 1,
	ETHTOOL_LINK_EXT_SUBSTATE_LLM_PCS_DID_NOT_ACQUIRE_AM_LOCK,
	ETHTOOL_LINK_EXT_SUBSTATE_LLM_PCS_DID_NOT_GET_ALIGN_STATUS,
	ETHTOOL_LINK_EXT_SUBSTATE_LLM_FC_FEC_IS_NOT_LOCKED,
	ETHTOOL_LINK_EXT_SUBSTATE_LLM_RS_FEC_IS_NOT_LOCKED,
};

enum ethtool_link_ext_substate_bad_signal_integrity {
	ETHTOOL_LINK_EXT_SUBSTATE_BSI_LARGE_NUMBER_OF_PHYSICAL_ERRORS = 1,
	ETHTOOL_LINK_EXT_SUBSTATE_BSI_UNSUPPORTED_RATE,
	ETHTOOL_LINK_EXT_SUBSTATE_BSI_SERDES_REFERENCE_CLOCK_LOST,
	ETHTOOL_LINK_EXT_SUBSTATE_BSI_SERDES_ALOS,
};

enum ethtool_link_ext_substate_cable_issue {
	ETHTOOL_LINK_EXT_SUBSTATE_CI_UNSUPPORTED_CABLE = 1,
	ETHTOOL_LINK_EXT_SUBSTATE_CI_CABLE_TEST_FAILURE,
};

enum ethtool_link_ext_substate_module {
	ETHTOOL_LINK_EXT_SUBSTATE_MODULE_CMIS_NOT_READY = 1,
};

#define ETH_GSTRING_LEN		32

enum ethtool_stringset {
	ETH_SS_TEST		= 0,
	ETH_SS_STATS,
	ETH_SS_PRIV_FLAGS,
	ETH_SS_NTUPLE_FILTERS,
	ETH_SS_FEATURES,
	ETH_SS_RSS_HASH_FUNCS,
	ETH_SS_TUNABLES,
	ETH_SS_PHY_STATS,
	ETH_SS_PHY_TUNABLES,
	ETH_SS_LINK_MODES,
	ETH_SS_MSG_CLASSES,
	ETH_SS_WOL_MODES,
	ETH_SS_SOF_TIMESTAMPING,
	ETH_SS_TS_TX_TYPES,
	ETH_SS_TS_RX_FILTERS,
	ETH_SS_UDP_TUNNEL_TYPES,
	ETH_SS_STATS_STD,
	ETH_SS_STATS_ETH_PHY,
	ETH_SS_STATS_ETH_MAC,
	ETH_SS_STATS_ETH_CTRL,
	ETH_SS_STATS_RMON,
	ETH_SS_STATS_PHY,
	ETH_SS_TS_FLAGS,

	ETH_SS_COUNT
};

enum ethtool_mac_stats_src {
	ETHTOOL_MAC_STATS_SRC_AGGREGATE,
	ETHTOOL_MAC_STATS_SRC_EMAC,
	ETHTOOL_MAC_STATS_SRC_PMAC,
};

enum ethtool_module_power_mode_policy {
	ETHTOOL_MODULE_POWER_MODE_POLICY_HIGH = 1,
	ETHTOOL_MODULE_POWER_MODE_POLICY_AUTO,
};

enum ethtool_module_power_mode {
	ETHTOOL_MODULE_POWER_MODE_LOW = 1,
	ETHTOOL_MODULE_POWER_MODE_HIGH,
};

enum ethtool_c33_pse_ext_state {
	ETHTOOL_C33_PSE_EXT_STATE_ERROR_CONDITION = 1,
	ETHTOOL_C33_PSE_EXT_STATE_MR_MPS_VALID,
	ETHTOOL_C33_PSE_EXT_STATE_MR_PSE_ENABLE,
	ETHTOOL_C33_PSE_EXT_STATE_OPTION_DETECT_TED,
	ETHTOOL_C33_PSE_EXT_STATE_OPTION_VPORT_LIM,
	ETHTOOL_C33_PSE_EXT_STATE_OVLD_DETECTED,
	ETHTOOL_C33_PSE_EXT_STATE_PD_DLL_POWER_TYPE,
	ETHTOOL_C33_PSE_EXT_STATE_POWER_NOT_AVAILABLE,
	ETHTOOL_C33_PSE_EXT_STATE_SHORT_DETECTED,
};

enum ethtool_c33_pse_ext_substate_mr_mps_valid {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_MR_MPS_VALID_DETECTED_UNDERLOAD = 1,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_MR_MPS_VALID_CONNECTION_OPEN,
};

enum ethtool_c33_pse_ext_substate_error_condition {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_NON_EXISTING_PORT = 1,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_UNDEFINED_PORT,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_INTERNAL_HW_FAULT,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_COMM_ERROR_AFTER_FORCE_ON,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_UNKNOWN_PORT_STATUS,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_HOST_CRASH_TURN_OFF,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_HOST_CRASH_FORCE_SHUTDOWN,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_CONFIG_CHANGE,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_ERROR_CONDITION_DETECTED_OVER_TEMP,
};

enum ethtool_c33_pse_ext_substate_mr_pse_enable {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_MR_PSE_ENABLE_DISABLE_PIN_ACTIVE = 1,
};

enum ethtool_c33_pse_ext_substate_option_detect_ted {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OPTION_DETECT_TED_DET_IN_PROCESS = 1,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OPTION_DETECT_TED_CONNECTION_CHECK_ERROR,
};

enum ethtool_c33_pse_ext_substate_option_vport_lim {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OPTION_VPORT_LIM_HIGH_VOLTAGE = 1,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OPTION_VPORT_LIM_LOW_VOLTAGE,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OPTION_VPORT_LIM_VOLTAGE_INJECTION,
};

enum ethtool_c33_pse_ext_substate_ovld_detected {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_OVLD_DETECTED_OVERLOAD = 1,
};

enum ethtool_c33_pse_ext_substate_power_not_available {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_POWER_NOT_AVAILABLE_BUDGET_EXCEEDED =  1,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_POWER_NOT_AVAILABLE_PORT_PW_LIMIT_EXCEEDS_CONTROLLER_BUDGET,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_POWER_NOT_AVAILABLE_PD_REQUEST_EXCEEDS_PORT_LIMIT,
	ETHTOOL_C33_PSE_EXT_SUBSTATE_POWER_NOT_AVAILABLE_HW_PW_LIMIT,
};

enum ethtool_c33_pse_ext_substate_short_detected {
	ETHTOOL_C33_PSE_EXT_SUBSTATE_SHORT_DETECTED_SHORT_CONDITION = 1,
};

enum ethtool_pse_types {
	ETHTOOL_PSE_UNKNOWN =	1 << 0,
	ETHTOOL_PSE_PODL =	1 << 1,
	ETHTOOL_PSE_C33 =	1 << 2,
};

enum ethtool_c33_pse_admin_state {
	ETHTOOL_C33_PSE_ADMIN_STATE_UNKNOWN = 1,
	ETHTOOL_C33_PSE_ADMIN_STATE_DISABLED,
	ETHTOOL_C33_PSE_ADMIN_STATE_ENABLED,
};

enum ethtool_c33_pse_pw_d_status {
	ETHTOOL_C33_PSE_PW_D_STATUS_UNKNOWN = 1,
	ETHTOOL_C33_PSE_PW_D_STATUS_DISABLED,
	ETHTOOL_C33_PSE_PW_D_STATUS_SEARCHING,
	ETHTOOL_C33_PSE_PW_D_STATUS_DELIVERING,
	ETHTOOL_C33_PSE_PW_D_STATUS_TEST,
	ETHTOOL_C33_PSE_PW_D_STATUS_FAULT,
	ETHTOOL_C33_PSE_PW_D_STATUS_OTHERFAULT,
};

enum ethtool_podl_pse_admin_state {
	ETHTOOL_PODL_PSE_ADMIN_STATE_UNKNOWN = 1,
	ETHTOOL_PODL_PSE_ADMIN_STATE_DISABLED,
	ETHTOOL_PODL_PSE_ADMIN_STATE_ENABLED,
};

enum ethtool_podl_pse_pw_d_status {
	ETHTOOL_PODL_PSE_PW_D_STATUS_UNKNOWN = 1,
	ETHTOOL_PODL_PSE_PW_D_STATUS_DISABLED,
	ETHTOOL_PODL_PSE_PW_D_STATUS_SEARCHING,
	ETHTOOL_PODL_PSE_PW_D_STATUS_DELIVERING,
	ETHTOOL_PODL_PSE_PW_D_STATUS_SLEEP,
	ETHTOOL_PODL_PSE_PW_D_STATUS_IDLE,
	ETHTOOL_PODL_PSE_PW_D_STATUS_ERROR,
};

enum ethtool_mm_verify_status {
	ETHTOOL_MM_VERIFY_STATUS_UNKNOWN,
	ETHTOOL_MM_VERIFY_STATUS_INITIAL,
	ETHTOOL_MM_VERIFY_STATUS_VERIFYING,
	ETHTOOL_MM_VERIFY_STATUS_SUCCEEDED,
	ETHTOOL_MM_VERIFY_STATUS_FAILED,
	ETHTOOL_MM_VERIFY_STATUS_DISABLED,
};

enum ethtool_module_fw_flash_status {
	ETHTOOL_MODULE_FW_FLASH_STATUS_STARTED = 1,
	ETHTOOL_MODULE_FW_FLASH_STATUS_IN_PROGRESS,
	ETHTOOL_MODULE_FW_FLASH_STATUS_COMPLETED,
	ETHTOOL_MODULE_FW_FLASH_STATUS_ERROR,
};

struct ethtool_gstrings {
	uint32_t	cmd;
	uint32_t	string_set;
	uint32_t	len;
	uint8_t	data[];
};

struct ethtool_sset_info {
	uint32_t	cmd;
	uint32_t	reserved;
	uint64_t	sset_mask;
	uint32_t	data[];
};


enum ethtool_test_flags {
	ETH_TEST_FL_OFFLINE	= (1 << 0),
	ETH_TEST_FL_FAILED	= (1 << 1),
	ETH_TEST_FL_EXTERNAL_LB	= (1 << 2),
	ETH_TEST_FL_EXTERNAL_LB_DONE	= (1 << 3),
};

struct ethtool_test {
	uint32_t	cmd;
	uint32_t	flags;
	uint32_t	reserved;
	uint32_t	len;
	uint64_t	data[];
};

struct ethtool_stats {
	uint32_t	cmd;
	uint32_t	n_stats;
	uint64_t	data[];
};

struct ethtool_perm_addr {
	uint32_t	cmd;
	uint32_t	size;
	uint8_t	data[];
};

enum ethtool_flags {
	ETH_FLAG_TXVLAN		= (1 << 7),	/* TX VLAN offload enabled */
	ETH_FLAG_RXVLAN		= (1 << 8),	/* RX VLAN offload enabled */
	ETH_FLAG_LRO		= (1 << 15),	/* LRO is enabled */
	ETH_FLAG_NTUPLE		= (1 << 27),	/* N-tuple filters enabled */
	ETH_FLAG_RXHASH		= (1 << 28),
};


struct ethtool_tcpip4_spec {
	uint32_t	ip4src;
	uint32_t	ip4dst;
	uint16_t	psrc;
	uint16_t	pdst;
	uint8_t    tos;
};

struct ethtool_ah_espip4_spec {
	uint32_t	ip4src;
	uint32_t	ip4dst;
	uint32_t	spi;
	uint8_t    tos;
};

#define	ETH_RX_NFC_IP4	1

struct ethtool_usrip4_spec {
	uint32_t	ip4src;
	uint32_t	ip4dst;
	uint32_t	l4_4_bytes;
	uint8_t    tos;
	uint8_t    ip_ver;
	uint8_t    proto;
};

struct ethtool_tcpip6_spec {
	uint32_t	ip6src[4];
	uint32_t	ip6dst[4];
	uint16_t	psrc;
	uint16_t	pdst;
	uint8_t    tclass;
};

struct ethtool_ah_espip6_spec {
	uint32_t	ip6src[4];
	uint32_t	ip6dst[4];
	uint32_t	spi;
	uint8_t    tclass;
};

struct ethtool_usrip6_spec {
	uint32_t	ip6src[4];
	uint32_t	ip6dst[4];
	uint32_t	l4_4_bytes;
	uint8_t    tclass;
	uint8_t    l4_proto;
};

union ethtool_flow_union {
	struct ethtool_tcpip4_spec		tcp_ip4_spec;
	struct ethtool_tcpip4_spec		udp_ip4_spec;
	struct ethtool_tcpip4_spec		sctp_ip4_spec;
	struct ethtool_ah_espip4_spec		ah_ip4_spec;
	struct ethtool_ah_espip4_spec		esp_ip4_spec;
	struct ethtool_usrip4_spec		usr_ip4_spec;
	struct ethtool_tcpip6_spec		tcp_ip6_spec;
	struct ethtool_tcpip6_spec		udp_ip6_spec;
	struct ethtool_tcpip6_spec		sctp_ip6_spec;
	struct ethtool_ah_espip6_spec		ah_ip6_spec;
	struct ethtool_ah_espip6_spec		esp_ip6_spec;
	struct ethtool_usrip6_spec		usr_ip6_spec;
	struct eth_header				ether_spec;
	uint8_t					hdata[52];
};

struct ethtool_flow_ext {
	uint8_t		padding[2];
	unsigned char	h_dest[ETH_ALEN];
	uint16_t		vlan_etype;
	uint16_t		vlan_tci;
	uint32_t		data[2];
};

struct ethtool_rx_flow_spec {
	uint32_t		flow_type;
	union ethtool_flow_union h_u;
	struct ethtool_flow_ext h_ext;
	union ethtool_flow_union m_u;
	struct ethtool_flow_ext m_ext;
	uint64_t		ring_cookie;
	uint32_t		location;
};

#define ETHTOOL_RX_FLOW_SPEC_RING	0x00000000FFFFFFFFLL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF	0x000000FF00000000LL
#define ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF 32
static inline uint64_t ethtool_get_flow_spec_ring(uint64_t ring_cookie)
{
	return ETHTOOL_RX_FLOW_SPEC_RING & ring_cookie;
}

static inline uint64_t ethtool_get_flow_spec_ring_vf(uint64_t ring_cookie)
{
	return (ETHTOOL_RX_FLOW_SPEC_RING_VF & ring_cookie) >>
				ETHTOOL_RX_FLOW_SPEC_RING_VF_OFF;
}

struct ethtool_rxnfc {
	uint32_t				cmd;
	uint32_t				flow_type;
	uint64_t				data;
	struct ethtool_rx_flow_spec	fs;
	union {
		uint32_t			rule_cnt;
		uint32_t			rss_context;
	};
	uint32_t				rule_locs[];
};


struct ethtool_rxfh_indir {
	uint32_t	cmd;
	uint32_t	size;
	uint32_t	ring_index[];
};

struct ethtool_rxfh {
	uint32_t   cmd;
	uint32_t	rss_context;
	uint32_t   indir_size;
	uint32_t   key_size;
	uint8_t	hfunc;
	uint8_t	input_xfrm;
	uint8_t	rsvd8[2];
	uint32_t	rsvd32;
	uint32_t   rss_config[];
};
#define ETH_RXFH_CONTEXT_ALLOC		0xffffffff
#define ETH_RXFH_INDIR_NO_CHANGE	0xffffffff

struct ethtool_rx_ntuple_flow_spec {
	uint32_t		 flow_type;
	union {
		struct ethtool_tcpip4_spec		tcp_ip4_spec;
		struct ethtool_tcpip4_spec		udp_ip4_spec;
		struct ethtool_tcpip4_spec		sctp_ip4_spec;
		struct ethtool_ah_espip4_spec		ah_ip4_spec;
		struct ethtool_ah_espip4_spec		esp_ip4_spec;
		struct ethtool_usrip4_spec		usr_ip4_spec;
		struct eth_header				ether_spec;
		uint8_t					hdata[72];
	} h_u, m_u;

	uint16_t	        vlan_tag;
	uint16_t	        vlan_tag_mask;
	uint64_t		data;
	uint64_t		data_mask;

	int32_t		action;
#define ETHTOOL_RXNTUPLE_ACTION_DROP	(-1)	/* drop packet */
#define ETHTOOL_RXNTUPLE_ACTION_CLEAR	(-2)	/* clear filter */
};

struct ethtool_rx_ntuple {
	uint32_t					cmd;
	struct ethtool_rx_ntuple_flow_spec	fs;
};

#define ETHTOOL_FLASH_MAX_FILENAME	128
enum ethtool_flash_op_type {
	ETHTOOL_FLASH_ALL_REGIONS	= 0,
};

struct ethtool_flash {
	uint32_t	cmd;
	uint32_t	region;
	char	data[ETHTOOL_FLASH_MAX_FILENAME];
};

struct ethtool_dump {
	uint32_t	cmd;
	uint32_t	version;
	uint32_t	flag;
	uint32_t	len;
	uint8_t	data[];
};

#define ETH_FW_DUMP_DISABLE 0


struct ethtool_get_features_block {
	uint32_t	available;
	uint32_t	requested;
	uint32_t	active;
	uint32_t	never_changed;
};

struct ethtool_gfeatures {
	uint32_t	cmd;
	uint32_t	size;
	struct ethtool_get_features_block features[];
};

struct ethtool_set_features_block {
	uint32_t	valid;
	uint32_t	requested;
};

struct ethtool_sfeatures {
	uint32_t	cmd;
	uint32_t	size;
	struct ethtool_set_features_block features[];
};

struct ethtool_ts_info {
	uint32_t	cmd;
	uint32_t	so_timestamping;
	int32_t	phc_index;
	uint32_t	tx_types;
	uint32_t	tx_reserved[3];
	uint32_t	rx_filters;
	uint32_t	rx_reserved[3];
};

enum ethtool_sfeatures_retval_bits {
	ETHTOOL_F_UNSUPPORTED__BIT,
	ETHTOOL_F_WISH__BIT,
	ETHTOOL_F_COMPAT__BIT,
};

#define ETHTOOL_F_UNSUPPORTED   (1 << ETHTOOL_F_UNSUPPORTED__BIT)
#define ETHTOOL_F_WISH          (1 << ETHTOOL_F_WISH__BIT)
#define ETHTOOL_F_COMPAT        (1 << ETHTOOL_F_COMPAT__BIT)

#define MAX_NUM_QUEUE		4096

struct ethtool_per_queue_op {
	uint32_t	cmd;
	uint32_t	sub_command;
	uint32_t	queue_mask[__KERNEL_DIV_ROUND_UP(MAX_NUM_QUEUE, 32)];
	char	data[];
};

struct ethtool_fecparam {
	uint32_t   cmd;
	uint32_t   active_fec;
	uint32_t   fec;
	uint32_t   reserved;
};

enum ethtool_fec_config_bits {
	ETHTOOL_FEC_NONE_BIT,
	ETHTOOL_FEC_AUTO_BIT,
	ETHTOOL_FEC_OFF_BIT,
	ETHTOOL_FEC_RS_BIT,
	ETHTOOL_FEC_BASER_BIT,
	ETHTOOL_FEC_LLRS_BIT,
};

#define ETHTOOL_FEC_NONE		(1 << ETHTOOL_FEC_NONE_BIT)
#define ETHTOOL_FEC_AUTO		(1 << ETHTOOL_FEC_AUTO_BIT)
#define ETHTOOL_FEC_OFF			(1 << ETHTOOL_FEC_OFF_BIT)
#define ETHTOOL_FEC_RS			(1 << ETHTOOL_FEC_RS_BIT)
#define ETHTOOL_FEC_BASER		(1 << ETHTOOL_FEC_BASER_BIT)
#define ETHTOOL_FEC_LLRS		(1 << ETHTOOL_FEC_LLRS_BIT)

#define ETHTOOL_GSET		0x00000001 /* DEPRECATED, Get settings.
					    * Please use ETHTOOL_GLINKSETTINGS
					    */
#define ETHTOOL_SSET		0x00000002 /* DEPRECATED, Set settings.
					    * Please use ETHTOOL_SLINKSETTINGS
					    */
#define ETHTOOL_GDRVINFO	0x00000003 /* Get driver info. */
#define ETHTOOL_GREGS		0x00000004 /* Get NIC registers. */
#define ETHTOOL_GWOL		0x00000005 /* Get wake-on-lan options. */
#define ETHTOOL_SWOL		0x00000006 /* Set wake-on-lan options. */
#define ETHTOOL_GMSGLVL		0x00000007 /* Get driver message level */
#define ETHTOOL_SMSGLVL		0x00000008 /* Set driver msg level. */
#define ETHTOOL_NWAY_RST	0x00000009 /* Restart autonegotiation. */
#define ETHTOOL_GLINK		0x0000000a
#define ETHTOOL_GEEPROM		0x0000000b /* Get EEPROM data */
#define ETHTOOL_SEEPROM		0x0000000c /* Set EEPROM data. */
#define ETHTOOL_GCOALESCE	0x0000000e /* Get coalesce config */
#define ETHTOOL_SCOALESCE	0x0000000f /* Set coalesce config. */
#define ETHTOOL_GRINGPARAM	0x00000010 /* Get ring parameters */
#define ETHTOOL_SRINGPARAM	0x00000011 /* Set ring parameters. */
#define ETHTOOL_GPAUSEPARAM	0x00000012 /* Get pause parameters */
#define ETHTOOL_SPAUSEPARAM	0x00000013 /* Set pause parameters. */
#define ETHTOOL_GRXCSUM		0x00000014 /* Get RX hw csum enable (ethtool_value) */
#define ETHTOOL_SRXCSUM		0x00000015 /* Set RX hw csum enable (ethtool_value) */
#define ETHTOOL_GTXCSUM		0x00000016 /* Get TX hw csum enable (ethtool_value) */
#define ETHTOOL_STXCSUM		0x00000017 /* Set TX hw csum enable (ethtool_value) */
#define ETHTOOL_GSG		0x00000018 /* Get scatter-gather enable
					    * (ethtool_value) */
#define ETHTOOL_SSG		0x00000019 /* Set scatter-gather enable
					    * (ethtool_value). */
#define ETHTOOL_TEST		0x0000001a /* execute NIC self-test. */
#define ETHTOOL_GSTRINGS	0x0000001b /* get specified string set */
#define ETHTOOL_PHYS_ID		0x0000001c /* identify the NIC */
#define ETHTOOL_GSTATS		0x0000001d /* get NIC-specific statistics */
#define ETHTOOL_GTSO		0x0000001e /* Get TSO enable (ethtool_value) */
#define ETHTOOL_STSO		0x0000001f /* Set TSO enable (ethtool_value) */
#define ETHTOOL_GPERMADDR	0x00000020 /* Get permanent hardware address */
#define ETHTOOL_GUFO		0x00000021 /* Get UFO enable (ethtool_value) */
#define ETHTOOL_SUFO		0x00000022 /* Set UFO enable (ethtool_value) */
#define ETHTOOL_GGSO		0x00000023 /* Get GSO enable (ethtool_value) */
#define ETHTOOL_SGSO		0x00000024 /* Set GSO enable (ethtool_value) */
#define ETHTOOL_GFLAGS		0x00000025 /* Get flags bitmap(ethtool_value) */
#define ETHTOOL_SFLAGS		0x00000026 /* Set flags bitmap(ethtool_value) */
#define ETHTOOL_GPFLAGS		0x00000027 /* Get driver-private flags bitmap */
#define ETHTOOL_SPFLAGS		0x00000028 /* Set driver-private flags bitmap */

#define ETHTOOL_GRXFH		0x00000029 /* Get RX flow hash configuration */
#define ETHTOOL_SRXFH		0x0000002a /* Set RX flow hash configuration */
#define ETHTOOL_GGRO		0x0000002b /* Get GRO enable (ethtool_value) */
#define ETHTOOL_SGRO		0x0000002c /* Set GRO enable (ethtool_value) */
#define ETHTOOL_GRXRINGS	0x0000002d /* Get RX rings available for LB */
#define ETHTOOL_GRXCLSRLCNT	0x0000002e /* Get RX class rule count */
#define ETHTOOL_GRXCLSRULE	0x0000002f /* Get RX classification rule */
#define ETHTOOL_GRXCLSRLALL	0x00000030 /* Get all RX classification rule */
#define ETHTOOL_SRXCLSRLDEL	0x00000031 /* Delete RX classification rule */
#define ETHTOOL_SRXCLSRLINS	0x00000032 /* Insert RX classification rule */
#define ETHTOOL_FLASHDEV	0x00000033 /* Flash firmware to device */
#define ETHTOOL_RESET		0x00000034 /* Reset hardware */
#define ETHTOOL_SRXNTUPLE	0x00000035 /* Add an n-tuple filter to device */
#define ETHTOOL_GRXNTUPLE	0x00000036 /* deprecated */
#define ETHTOOL_GSSET_INFO	0x00000037 /* Get string set info */
#define ETHTOOL_GRXFHINDIR	0x00000038 /* Get RX flow hash indir'n table */
#define ETHTOOL_SRXFHINDIR	0x00000039 /* Set RX flow hash indir'n table */

#define ETHTOOL_GFEATURES	0x0000003a /* Get device offload settings */
#define ETHTOOL_SFEATURES	0x0000003b /* Change device offload settings */
#define ETHTOOL_GCHANNELS	0x0000003c /* Get no of channels */
#define ETHTOOL_SCHANNELS	0x0000003d /* Set no of channels */
#define ETHTOOL_SET_DUMP	0x0000003e /* Set dump settings */
#define ETHTOOL_GET_DUMP_FLAG	0x0000003f /* Get dump settings */
#define ETHTOOL_GET_DUMP_DATA	0x00000040 /* Get dump data */
#define ETHTOOL_GET_TS_INFO	0x00000041 /* Get time stamping and PHC info */
#define ETHTOOL_GMODULEINFO	0x00000042 /* Get plug-in module information */
#define ETHTOOL_GMODULEEEPROM	0x00000043 /* Get plug-in module eeprom */
#define ETHTOOL_GEEE		0x00000044 /* Get EEE settings */
#define ETHTOOL_SEEE		0x00000045 /* Set EEE settings */

#define ETHTOOL_GRSSH		0x00000046 /* Get RX flow hash configuration */
#define ETHTOOL_SRSSH		0x00000047 /* Set RX flow hash configuration */
#define ETHTOOL_GTUNABLE	0x00000048 /* Get tunable configuration */
#define ETHTOOL_STUNABLE	0x00000049 /* Set tunable configuration */
#define ETHTOOL_GPHYSTATS	0x0000004a /* get PHY-specific statistics */

#define ETHTOOL_PERQUEUE	0x0000004b /* Set per queue options */

#define ETHTOOL_GLINKSETTINGS	0x0000004c /* Get ethtool_link_settings */
#define ETHTOOL_SLINKSETTINGS	0x0000004d /* Set ethtool_link_settings */
#define ETHTOOL_PHY_GTUNABLE	0x0000004e /* Get PHY tunable configuration */
#define ETHTOOL_PHY_STUNABLE	0x0000004f /* Set PHY tunable configuration */
#define ETHTOOL_GFECPARAM	0x00000050 /* Get FEC settings */
#define ETHTOOL_SFECPARAM	0x00000051 /* Set FEC settings */

#define SPARC_ETH_GSET		ETHTOOL_GSET
#define SPARC_ETH_SSET		ETHTOOL_SSET

enum ethtool_link_mode_bit_indices {
	ETHTOOL_LINK_MODE_10baseT_Half_BIT	= 0,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT	= 1,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT	= 2,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT	= 3,
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT	= 4,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT	= 5,
	ETHTOOL_LINK_MODE_Autoneg_BIT		= 6,
	ETHTOOL_LINK_MODE_TP_BIT		= 7,
	ETHTOOL_LINK_MODE_AUI_BIT		= 8,
	ETHTOOL_LINK_MODE_MII_BIT		= 9,
	ETHTOOL_LINK_MODE_FIBRE_BIT		= 10,
	ETHTOOL_LINK_MODE_BNC_BIT		= 11,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT	= 12,
	ETHTOOL_LINK_MODE_Pause_BIT		= 13,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT	= 14,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT	= 15,
	ETHTOOL_LINK_MODE_Backplane_BIT		= 16,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT	= 17,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT	= 18,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT	= 19,
	ETHTOOL_LINK_MODE_10000baseR_FEC_BIT	= 20,
	ETHTOOL_LINK_MODE_20000baseMLD2_Full_BIT = 21,
	ETHTOOL_LINK_MODE_20000baseKR2_Full_BIT	= 22,
	ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT	= 23,
	ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT	= 24,
	ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT	= 25,
	ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT	= 26,
	ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT	= 27,
	ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT	= 28,
	ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT	= 29,
	ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT	= 30,
	ETHTOOL_LINK_MODE_25000baseCR_Full_BIT	= 31,


	ETHTOOL_LINK_MODE_25000baseKR_Full_BIT	= 32,
	ETHTOOL_LINK_MODE_25000baseSR_Full_BIT	= 33,
	ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT	= 34,
	ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT	= 35,
	ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT	= 36,
	ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT	= 37,
	ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT	= 38,
	ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT	= 39,
	ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT		= 40,
	ETHTOOL_LINK_MODE_1000baseX_Full_BIT	= 41,
	ETHTOOL_LINK_MODE_10000baseCR_Full_BIT	= 42,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT	= 43,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT	= 44,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT	= 45,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT	= 46,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT	= 47,
	ETHTOOL_LINK_MODE_5000baseT_Full_BIT	= 48,

	ETHTOOL_LINK_MODE_FEC_NONE_BIT	= 49,
	ETHTOOL_LINK_MODE_FEC_RS_BIT	= 50,
	ETHTOOL_LINK_MODE_FEC_BASER_BIT	= 51,
	ETHTOOL_LINK_MODE_50000baseKR_Full_BIT		 = 52,
	ETHTOOL_LINK_MODE_50000baseSR_Full_BIT		 = 53,
	ETHTOOL_LINK_MODE_50000baseCR_Full_BIT		 = 54,
	ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT	 = 55,
	ETHTOOL_LINK_MODE_50000baseDR_Full_BIT		 = 56,
	ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT	 = 57,
	ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT	 = 58,
	ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT	 = 59,
	ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT = 60,
	ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT	 = 61,
	ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT	 = 62,
	ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT	 = 63,
	ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT = 64,
	ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT	 = 65,
	ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT	 = 66,
	ETHTOOL_LINK_MODE_100baseT1_Full_BIT		 = 67,
	ETHTOOL_LINK_MODE_1000baseT1_Full_BIT		 = 68,
	ETHTOOL_LINK_MODE_400000baseKR8_Full_BIT	 = 69,
	ETHTOOL_LINK_MODE_400000baseSR8_Full_BIT	 = 70,
	ETHTOOL_LINK_MODE_400000baseLR8_ER8_FR8_Full_BIT = 71,
	ETHTOOL_LINK_MODE_400000baseDR8_Full_BIT	 = 72,
	ETHTOOL_LINK_MODE_400000baseCR8_Full_BIT	 = 73,
	ETHTOOL_LINK_MODE_FEC_LLRS_BIT			 = 74,
	ETHTOOL_LINK_MODE_100000baseKR_Full_BIT		 = 75,
	ETHTOOL_LINK_MODE_100000baseSR_Full_BIT		 = 76,
	ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT	 = 77,
	ETHTOOL_LINK_MODE_100000baseCR_Full_BIT		 = 78,
	ETHTOOL_LINK_MODE_100000baseDR_Full_BIT		 = 79,
	ETHTOOL_LINK_MODE_200000baseKR2_Full_BIT	 = 80,
	ETHTOOL_LINK_MODE_200000baseSR2_Full_BIT	 = 81,
	ETHTOOL_LINK_MODE_200000baseLR2_ER2_FR2_Full_BIT = 82,
	ETHTOOL_LINK_MODE_200000baseDR2_Full_BIT	 = 83,
	ETHTOOL_LINK_MODE_200000baseCR2_Full_BIT	 = 84,
	ETHTOOL_LINK_MODE_400000baseKR4_Full_BIT	 = 85,
	ETHTOOL_LINK_MODE_400000baseSR4_Full_BIT	 = 86,
	ETHTOOL_LINK_MODE_400000baseLR4_ER4_FR4_Full_BIT = 87,
	ETHTOOL_LINK_MODE_400000baseDR4_Full_BIT	 = 88,
	ETHTOOL_LINK_MODE_400000baseCR4_Full_BIT	 = 89,
	ETHTOOL_LINK_MODE_100baseFX_Half_BIT		 = 90,
	ETHTOOL_LINK_MODE_100baseFX_Full_BIT		 = 91,
	ETHTOOL_LINK_MODE_10baseT1L_Full_BIT		 = 92,
	ETHTOOL_LINK_MODE_800000baseCR8_Full_BIT	 = 93,
	ETHTOOL_LINK_MODE_800000baseKR8_Full_BIT	 = 94,
	ETHTOOL_LINK_MODE_800000baseDR8_Full_BIT	 = 95,
	ETHTOOL_LINK_MODE_800000baseDR8_2_Full_BIT	 = 96,
	ETHTOOL_LINK_MODE_800000baseSR8_Full_BIT	 = 97,
	ETHTOOL_LINK_MODE_800000baseVR8_Full_BIT	 = 98,
	ETHTOOL_LINK_MODE_10baseT1S_Full_BIT		 = 99,
	ETHTOOL_LINK_MODE_10baseT1S_Half_BIT		 = 100,
	ETHTOOL_LINK_MODE_10baseT1S_P2MP_Half_BIT	 = 101,
	ETHTOOL_LINK_MODE_10baseT1BRR_Full_BIT		 = 102,

	__ETHTOOL_LINK_MODE_MASK_NBITS
};

#define __ETHTOOL_LINK_MODE_LEGACY_MASK(base_name)	\
	(1UL << (ETHTOOL_LINK_MODE_ ## base_name ## _BIT))

#define SUPPORTED_10baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define SUPPORTED_10baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define SUPPORTED_100baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define SUPPORTED_100baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define SUPPORTED_1000baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define SUPPORTED_1000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define SUPPORTED_Autoneg		__ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define SUPPORTED_TP			__ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define SUPPORTED_AUI			__ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define SUPPORTED_MII			__ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define SUPPORTED_FIBRE			__ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define SUPPORTED_BNC			__ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define SUPPORTED_10000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define SUPPORTED_Pause			__ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define SUPPORTED_Asym_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define SUPPORTED_2500baseX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define SUPPORTED_Backplane		__ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define SUPPORTED_1000baseKX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define SUPPORTED_10000baseKX4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define SUPPORTED_10000baseKR_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define SUPPORTED_10000baseR_FEC	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define SUPPORTED_20000baseMLD2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define SUPPORTED_20000baseKR2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define SUPPORTED_40000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define SUPPORTED_40000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define SUPPORTED_40000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define SUPPORTED_40000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define SUPPORTED_56000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define SUPPORTED_56000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define SUPPORTED_56000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define SUPPORTED_56000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)

#define ADVERTISED_10baseT_Half		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Half)
#define ADVERTISED_10baseT_Full		__ETHTOOL_LINK_MODE_LEGACY_MASK(10baseT_Full)
#define ADVERTISED_100baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Half)
#define ADVERTISED_100baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(100baseT_Full)
#define ADVERTISED_1000baseT_Half	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Half)
#define ADVERTISED_1000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseT_Full)
#define ADVERTISED_Autoneg		__ETHTOOL_LINK_MODE_LEGACY_MASK(Autoneg)
#define ADVERTISED_TP			__ETHTOOL_LINK_MODE_LEGACY_MASK(TP)
#define ADVERTISED_AUI			__ETHTOOL_LINK_MODE_LEGACY_MASK(AUI)
#define ADVERTISED_MII			__ETHTOOL_LINK_MODE_LEGACY_MASK(MII)
#define ADVERTISED_FIBRE		__ETHTOOL_LINK_MODE_LEGACY_MASK(FIBRE)
#define ADVERTISED_BNC			__ETHTOOL_LINK_MODE_LEGACY_MASK(BNC)
#define ADVERTISED_10000baseT_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseT_Full)
#define ADVERTISED_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Pause)
#define ADVERTISED_Asym_Pause		__ETHTOOL_LINK_MODE_LEGACY_MASK(Asym_Pause)
#define ADVERTISED_2500baseX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(2500baseX_Full)
#define ADVERTISED_Backplane		__ETHTOOL_LINK_MODE_LEGACY_MASK(Backplane)
#define ADVERTISED_1000baseKX_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(1000baseKX_Full)
#define ADVERTISED_10000baseKX4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKX4_Full)
#define ADVERTISED_10000baseKR_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseKR_Full)
#define ADVERTISED_10000baseR_FEC	__ETHTOOL_LINK_MODE_LEGACY_MASK(10000baseR_FEC)
#define ADVERTISED_20000baseMLD2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseMLD2_Full)
#define ADVERTISED_20000baseKR2_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(20000baseKR2_Full)
#define ADVERTISED_40000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseKR4_Full)
#define ADVERTISED_40000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseCR4_Full)
#define ADVERTISED_40000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseSR4_Full)
#define ADVERTISED_40000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(40000baseLR4_Full)
#define ADVERTISED_56000baseKR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseKR4_Full)
#define ADVERTISED_56000baseCR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseCR4_Full)
#define ADVERTISED_56000baseSR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseSR4_Full)
#define ADVERTISED_56000baseLR4_Full	__ETHTOOL_LINK_MODE_LEGACY_MASK(56000baseLR4_Full)


#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_2500		2500
#define SPEED_5000		5000
#define SPEED_10000		10000
#define SPEED_14000		14000
#define SPEED_20000		20000
#define SPEED_25000		25000
#define SPEED_40000		40000
#define SPEED_50000		50000
#define SPEED_56000		56000
#define SPEED_100000		100000
#define SPEED_200000		200000
#define SPEED_400000		400000
#define SPEED_800000		800000

#define SPEED_UNKNOWN		-1

static inline int ethtool_validate_speed(uint32_t speed)
{
	return speed <= INT_MAX || speed == (uint32_t)SPEED_UNKNOWN;
}

#define DUPLEX_HALF		0x00
#define DUPLEX_FULL		0x01
#define DUPLEX_UNKNOWN		0xff

static inline int ethtool_validate_duplex(uint8_t duplex)
{
	switch (duplex) {
	case DUPLEX_HALF:
	case DUPLEX_FULL:
	case DUPLEX_UNKNOWN:
		return 1;
	}

	return 0;
}

#define MASTER_SLAVE_CFG_UNSUPPORTED		0
#define MASTER_SLAVE_CFG_UNKNOWN		1
#define MASTER_SLAVE_CFG_MASTER_PREFERRED	2
#define MASTER_SLAVE_CFG_SLAVE_PREFERRED	3
#define MASTER_SLAVE_CFG_MASTER_FORCE		4
#define MASTER_SLAVE_CFG_SLAVE_FORCE		5
#define MASTER_SLAVE_STATE_UNSUPPORTED		0
#define MASTER_SLAVE_STATE_UNKNOWN		1
#define MASTER_SLAVE_STATE_MASTER		2
#define MASTER_SLAVE_STATE_SLAVE		3
#define MASTER_SLAVE_STATE_ERR			4

#define RATE_MATCH_NONE		0
#define RATE_MATCH_PAUSE	1
#define RATE_MATCH_CRS		2
#define RATE_MATCH_OPEN_LOOP	3

#define PORT_TP			0x00
#define PORT_AUI		0x01
#define PORT_MII		0x02
#define PORT_FIBRE		0x03
#define PORT_BNC		0x04
#define PORT_DA			0x05
#define PORT_NONE		0xef
#define PORT_OTHER		0xff

#define XCVR_INTERNAL		0x00 /* PHY and MAC are in the same package */
#define XCVR_EXTERNAL		0x01 /* PHY and MAC are in different packages */
#define XCVR_DUMMY1		0x02
#define XCVR_DUMMY2		0x03
#define XCVR_DUMMY3		0x04

#define AUTONEG_DISABLE		0x00
#define AUTONEG_ENABLE		0x01

#define ETH_TP_MDI_INVALID	0x00 /* status: unknown; control: unsupported */
#define ETH_TP_MDI		0x01 /* status: MDI;     control: force MDI */
#define ETH_TP_MDI_X		0x02 /* status: MDI-X;   control: force MDI-X */
#define ETH_TP_MDI_AUTO		0x03 /*                  control: auto-select */

#define WAKE_PHY		(1 << 0)
#define WAKE_UCAST		(1 << 1)
#define WAKE_MCAST		(1 << 2)
#define WAKE_BCAST		(1 << 3)
#define WAKE_ARP		(1 << 4)
#define WAKE_MAGIC		(1 << 5)
#define WAKE_MAGICSECURE	(1 << 6) /* only meaningful if WAKE_MAGIC */
#define WAKE_FILTER		(1 << 7)

#define WOL_MODE_COUNT		8

#define	RXH_XFRM_SYM_XOR	(1 << 0)
#define	RXH_XFRM_NO_CHANGE	0xff

#define	TCP_V4_FLOW	0x01	/* hash or spec (tcp_ip4_spec) */
#define	UDP_V4_FLOW	0x02	/* hash or spec (udp_ip4_spec) */
#define	SCTP_V4_FLOW	0x03	/* hash or spec (sctp_ip4_spec) */
#define	AH_ESP_V4_FLOW	0x04	/* hash only */
#define	TCP_V6_FLOW	0x05	/* hash or spec (tcp_ip6_spec; nfc only) */
#define	UDP_V6_FLOW	0x06	/* hash or spec (udp_ip6_spec; nfc only) */
#define	SCTP_V6_FLOW	0x07	/* hash or spec (sctp_ip6_spec; nfc only) */
#define	AH_ESP_V6_FLOW	0x08	/* hash only */
#define	AH_V4_FLOW	0x09	/* hash or spec (ah_ip4_spec) */
#define	ESP_V4_FLOW	0x0a	/* hash or spec (esp_ip4_spec) */
#define	AH_V6_FLOW	0x0b	/* hash or spec (ah_ip6_spec; nfc only) */
#define	ESP_V6_FLOW	0x0c	/* hash or spec (esp_ip6_spec; nfc only) */
#define	IPV4_USER_FLOW	0x0d	/* spec only (usr_ip4_spec) */
#define	IP_USER_FLOW	IPV4_USER_FLOW
#define	IPV6_USER_FLOW	0x0e	/* spec only (usr_ip6_spec; nfc only) */
#define	IPV4_FLOW	0x10	/* hash only */
#define	IPV6_FLOW	0x11	/* hash only */
#define	ETHER_FLOW	0x12	/* spec only (ether_spec) */

#define GTPU_V4_FLOW 0x13	/* hash only */
#define GTPU_V6_FLOW 0x14	/* hash only */

#define GTPC_V4_FLOW 0x15	/* hash only */
#define GTPC_V6_FLOW 0x16	/* hash only */

#define GTPC_TEID_V4_FLOW 0x17	/* hash only */
#define GTPC_TEID_V6_FLOW 0x18	/* hash only */

#define GTPU_EH_V4_FLOW 0x19	/* hash only */
#define GTPU_EH_V6_FLOW 0x1a	/* hash only */

#define GTPU_UL_V4_FLOW 0x1b	/* hash only */
#define GTPU_UL_V6_FLOW 0x1c	/* hash only */
#define GTPU_DL_V4_FLOW 0x1d	/* hash only */
#define GTPU_DL_V6_FLOW 0x1e	/* hash only */

#define	FLOW_EXT	0x80000000
#define	FLOW_MAC_EXT	0x40000000
#define	FLOW_RSS	0x20000000

#define	RXH_L2DA	(1 << 1)
#define	RXH_VLAN	(1 << 2)
#define	RXH_L3_PROTO	(1 << 3)
#define	RXH_IP_SRC	(1 << 4)
#define	RXH_IP_DST	(1 << 5)
#define	RXH_L4_B_0_1	(1 << 6) /* src port in case of TCP/UDP/SCTP */
#define	RXH_L4_B_2_3	(1 << 7) /* dst port in case of TCP/UDP/SCTP */
#define	RXH_GTP_TEID	(1 << 8) /* teid in case of GTP */
#define	RXH_DISCARD	(1 << 31)

#define	RX_CLS_FLOW_DISC	0xffffffffffffffffULL
#define RX_CLS_FLOW_WAKE	0xfffffffffffffffeULL

#define RX_CLS_LOC_SPECIAL	0x80000000	/* flag */
#define RX_CLS_LOC_ANY		0xffffffff
#define RX_CLS_LOC_FIRST	0xfffffffe
#define RX_CLS_LOC_LAST		0xfffffffd

#define ETH_MODULE_SFF_8079		0x1
#define ETH_MODULE_SFF_8079_LEN		256
#define ETH_MODULE_SFF_8472		0x2
#define ETH_MODULE_SFF_8472_LEN		512
#define ETH_MODULE_SFF_8636		0x3
#define ETH_MODULE_SFF_8636_LEN		256
#define ETH_MODULE_SFF_8436		0x4
#define ETH_MODULE_SFF_8436_LEN		256

#define ETH_MODULE_SFF_8636_MAX_LEN     640
#define ETH_MODULE_SFF_8436_MAX_LEN     640

enum ethtool_reset_flags {
	ETH_RESET_MGMT		= 1 << 0,	/* Management processor */
	ETH_RESET_IRQ		= 1 << 1,	/* Interrupt requester */
	ETH_RESET_DMA		= 1 << 2,	/* DMA engine */
	ETH_RESET_FILTER	= 1 << 3,	/* Filtering/flow direction */
	ETH_RESET_OFFLOAD	= 1 << 4,	/* Protocol offload */
	ETH_RESET_MAC		= 1 << 5,	/* Media access controller */
	ETH_RESET_PHY		= 1 << 6,	/* Transceiver/PHY */
	ETH_RESET_RAM		= 1 << 7,	/* RAM shared between
						 * multiple components */
	ETH_RESET_AP		= 1 << 8,	/* Application processor */

	ETH_RESET_DEDICATED	= 0x0000ffff,	/* All components dedicated to
						 * this interface */
	ETH_RESET_ALL		= 0xffffffff,	/* All components used by this
						 * interface, even if shared */
};
#define ETH_RESET_SHARED_SHIFT	16


struct ethtool_link_settings {
	uint32_t	cmd;
	uint32_t	speed;
	uint8_t	duplex;
	uint8_t	port;
	uint8_t	phy_address;
	uint8_t	autoneg;
	uint8_t	mdio_support;
	uint8_t	eth_tp_mdix;
	uint8_t	eth_tp_mdix_ctrl;
	int8_t	link_mode_masks_nwords;
	uint8_t	transceiver;
	uint8_t	master_slave_cfg;
	uint8_t	master_slave_state;
	uint8_t	rate_matching;
	uint32_t	reserved[7];
	uint32_t	link_mode_masks[];
};

enum phy_upstream {
	PHY_UPSTREAM_MAC,
	PHY_UPSTREAM_PHY,
};

#endif /* _LINUX_ETHTOOL_H */
