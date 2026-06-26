#ifndef VIRTIO_SND_IF_H
#define VIRTIO_SND_IF_H

#include "standard-headers/linux/virtio_types.h"

enum {
	VIRTIO_SND_F_CTLS = 0
};

struct virtio_snd_config {
	uint32_t jacks;
	uint32_t streams;
	uint32_t chmaps;
	uint32_t controls;
};

enum {
	VIRTIO_SND_VQ_CONTROL = 0,
	VIRTIO_SND_VQ_EVENT,
	VIRTIO_SND_VQ_TX,
	VIRTIO_SND_VQ_RX,
	VIRTIO_SND_VQ_MAX
};


enum {
	VIRTIO_SND_D_OUTPUT = 0,
	VIRTIO_SND_D_INPUT
};

enum {
	VIRTIO_SND_R_JACK_INFO = 1,
	VIRTIO_SND_R_JACK_REMAP,

	VIRTIO_SND_R_PCM_INFO = 0x0100,
	VIRTIO_SND_R_PCM_SET_PARAMS,
	VIRTIO_SND_R_PCM_PREPARE,
	VIRTIO_SND_R_PCM_RELEASE,
	VIRTIO_SND_R_PCM_START,
	VIRTIO_SND_R_PCM_STOP,

	VIRTIO_SND_R_CHMAP_INFO = 0x0200,

	VIRTIO_SND_R_CTL_INFO = 0x0300,
	VIRTIO_SND_R_CTL_ENUM_ITEMS,
	VIRTIO_SND_R_CTL_READ,
	VIRTIO_SND_R_CTL_WRITE,
	VIRTIO_SND_R_CTL_TLV_READ,
	VIRTIO_SND_R_CTL_TLV_WRITE,
	VIRTIO_SND_R_CTL_TLV_COMMAND,

	VIRTIO_SND_EVT_JACK_CONNECTED = 0x1000,
	VIRTIO_SND_EVT_JACK_DISCONNECTED,

	VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED = 0x1100,
	VIRTIO_SND_EVT_PCM_XRUN,

	VIRTIO_SND_EVT_CTL_NOTIFY = 0x1200,

	VIRTIO_SND_S_OK = 0x8000,
	VIRTIO_SND_S_BAD_MSG,
	VIRTIO_SND_S_NOT_SUPP,
	VIRTIO_SND_S_IO_ERR
};

struct virtio_snd_hdr {
	uint32_t code;
};

struct virtio_snd_event {
	struct virtio_snd_hdr hdr;
	uint32_t data;
};

struct virtio_snd_query_info {
	struct virtio_snd_hdr hdr;
	uint32_t start_id;
	uint32_t count;
	uint32_t size;
};

struct virtio_snd_info {
	uint32_t hda_fn_nid;
};

struct virtio_snd_jack_hdr {
	struct virtio_snd_hdr hdr;
	uint32_t jack_id;
};

enum {
	VIRTIO_SND_JACK_F_REMAP = 0
};

struct virtio_snd_jack_info {
	struct virtio_snd_info hdr;
	uint32_t features;
	uint32_t hda_reg_defconf;
	uint32_t hda_reg_caps;
	uint8_t connected;

	uint8_t padding[7];
};

struct virtio_snd_jack_remap {
	struct virtio_snd_jack_hdr hdr;
	uint32_t association;
	uint32_t sequence;
};

struct virtio_snd_pcm_hdr {
	struct virtio_snd_hdr hdr;
	uint32_t stream_id;
};

enum {
	VIRTIO_SND_PCM_F_SHMEM_HOST = 0,
	VIRTIO_SND_PCM_F_SHMEM_GUEST,
	VIRTIO_SND_PCM_F_MSG_POLLING,
	VIRTIO_SND_PCM_F_EVT_SHMEM_PERIODS,
	VIRTIO_SND_PCM_F_EVT_XRUNS
};

enum {
	VIRTIO_SND_PCM_FMT_IMA_ADPCM = 0,	/*  4 /  4 bits */
	VIRTIO_SND_PCM_FMT_MU_LAW,		/*  8 /  8 bits */
	VIRTIO_SND_PCM_FMT_A_LAW,		/*  8 /  8 bits */
	VIRTIO_SND_PCM_FMT_S8,			/*  8 /  8 bits */
	VIRTIO_SND_PCM_FMT_U8,			/*  8 /  8 bits */
	VIRTIO_SND_PCM_FMT_S16,			/* 16 / 16 bits */
	VIRTIO_SND_PCM_FMT_U16,			/* 16 / 16 bits */
	VIRTIO_SND_PCM_FMT_S18_3,		/* 18 / 24 bits */
	VIRTIO_SND_PCM_FMT_U18_3,		/* 18 / 24 bits */
	VIRTIO_SND_PCM_FMT_S20_3,		/* 20 / 24 bits */
	VIRTIO_SND_PCM_FMT_U20_3,		/* 20 / 24 bits */
	VIRTIO_SND_PCM_FMT_S24_3,		/* 24 / 24 bits */
	VIRTIO_SND_PCM_FMT_U24_3,		/* 24 / 24 bits */
	VIRTIO_SND_PCM_FMT_S20,			/* 20 / 32 bits */
	VIRTIO_SND_PCM_FMT_U20,			/* 20 / 32 bits */
	VIRTIO_SND_PCM_FMT_S24,			/* 24 / 32 bits */
	VIRTIO_SND_PCM_FMT_U24,			/* 24 / 32 bits */
	VIRTIO_SND_PCM_FMT_S32,			/* 32 / 32 bits */
	VIRTIO_SND_PCM_FMT_U32,			/* 32 / 32 bits */
	VIRTIO_SND_PCM_FMT_FLOAT,		/* 32 / 32 bits */
	VIRTIO_SND_PCM_FMT_FLOAT64,		/* 64 / 64 bits */
	VIRTIO_SND_PCM_FMT_DSD_U8,		/*  8 /  8 bits */
	VIRTIO_SND_PCM_FMT_DSD_U16,		/* 16 / 16 bits */
	VIRTIO_SND_PCM_FMT_DSD_U32,		/* 32 / 32 bits */
	VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME	/* 32 / 32 bits */
};

enum {
	VIRTIO_SND_PCM_RATE_5512 = 0,
	VIRTIO_SND_PCM_RATE_8000,
	VIRTIO_SND_PCM_RATE_11025,
	VIRTIO_SND_PCM_RATE_16000,
	VIRTIO_SND_PCM_RATE_22050,
	VIRTIO_SND_PCM_RATE_32000,
	VIRTIO_SND_PCM_RATE_44100,
	VIRTIO_SND_PCM_RATE_48000,
	VIRTIO_SND_PCM_RATE_64000,
	VIRTIO_SND_PCM_RATE_88200,
	VIRTIO_SND_PCM_RATE_96000,
	VIRTIO_SND_PCM_RATE_176400,
	VIRTIO_SND_PCM_RATE_192000,
	VIRTIO_SND_PCM_RATE_384000
};

struct virtio_snd_pcm_info {
	struct virtio_snd_info hdr;
	uint32_t features;
	uint64_t formats;
	uint64_t rates;
	uint8_t direction;
	uint8_t channels_min;
	uint8_t channels_max;

	uint8_t padding[5];
};

struct virtio_snd_pcm_set_params {
	struct virtio_snd_pcm_hdr hdr;
	uint32_t buffer_bytes;
	uint32_t period_bytes;
	uint32_t features;
	uint8_t channels;
	uint8_t format;
	uint8_t rate;

	uint8_t padding;
};


struct virtio_snd_pcm_xfer {
	uint32_t stream_id;
};

struct virtio_snd_pcm_status {
	uint32_t status;
	uint32_t latency_bytes;
};

struct virtio_snd_chmap_hdr {
	struct virtio_snd_hdr hdr;
	uint32_t chmap_id;
};

enum {
	VIRTIO_SND_CHMAP_NONE = 0,	/* undefined */
	VIRTIO_SND_CHMAP_NA,		/* silent */
	VIRTIO_SND_CHMAP_MONO,		/* mono stream */
	VIRTIO_SND_CHMAP_FL,		/* front left */
	VIRTIO_SND_CHMAP_FR,		/* front right */
	VIRTIO_SND_CHMAP_RL,		/* rear left */
	VIRTIO_SND_CHMAP_RR,		/* rear right */
	VIRTIO_SND_CHMAP_FC,		/* front center */
	VIRTIO_SND_CHMAP_LFE,		/* low frequency (LFE) */
	VIRTIO_SND_CHMAP_SL,		/* side left */
	VIRTIO_SND_CHMAP_SR,		/* side right */
	VIRTIO_SND_CHMAP_RC,		/* rear center */
	VIRTIO_SND_CHMAP_FLC,		/* front left center */
	VIRTIO_SND_CHMAP_FRC,		/* front right center */
	VIRTIO_SND_CHMAP_RLC,		/* rear left center */
	VIRTIO_SND_CHMAP_RRC,		/* rear right center */
	VIRTIO_SND_CHMAP_FLW,		/* front left wide */
	VIRTIO_SND_CHMAP_FRW,		/* front right wide */
	VIRTIO_SND_CHMAP_FLH,		/* front left high */
	VIRTIO_SND_CHMAP_FCH,		/* front center high */
	VIRTIO_SND_CHMAP_FRH,		/* front right high */
	VIRTIO_SND_CHMAP_TC,		/* top center */
	VIRTIO_SND_CHMAP_TFL,		/* top front left */
	VIRTIO_SND_CHMAP_TFR,		/* top front right */
	VIRTIO_SND_CHMAP_TFC,		/* top front center */
	VIRTIO_SND_CHMAP_TRL,		/* top rear left */
	VIRTIO_SND_CHMAP_TRR,		/* top rear right */
	VIRTIO_SND_CHMAP_TRC,		/* top rear center */
	VIRTIO_SND_CHMAP_TFLC,		/* top front left center */
	VIRTIO_SND_CHMAP_TFRC,		/* top front right center */
	VIRTIO_SND_CHMAP_TSL,		/* top side left */
	VIRTIO_SND_CHMAP_TSR,		/* top side right */
	VIRTIO_SND_CHMAP_LLFE,		/* left LFE */
	VIRTIO_SND_CHMAP_RLFE,		/* right LFE */
	VIRTIO_SND_CHMAP_BC,		/* bottom center */
	VIRTIO_SND_CHMAP_BLC,		/* bottom left center */
	VIRTIO_SND_CHMAP_BRC		/* bottom right center */
};

#define VIRTIO_SND_CHMAP_MAX_SIZE	18

struct virtio_snd_chmap_info {
	struct virtio_snd_info hdr;
	uint8_t direction;
	uint8_t channels;
	uint8_t positions[VIRTIO_SND_CHMAP_MAX_SIZE];
};

struct virtio_snd_ctl_hdr {
	struct virtio_snd_hdr hdr;
	uint32_t control_id;
};

enum {
	VIRTIO_SND_CTL_ROLE_UNDEFINED = 0,
	VIRTIO_SND_CTL_ROLE_VOLUME,
	VIRTIO_SND_CTL_ROLE_MUTE,
	VIRTIO_SND_CTL_ROLE_GAIN
};

enum {
	VIRTIO_SND_CTL_TYPE_BOOLEAN = 0,
	VIRTIO_SND_CTL_TYPE_INTEGER,
	VIRTIO_SND_CTL_TYPE_INTEGER64,
	VIRTIO_SND_CTL_TYPE_ENUMERATED,
	VIRTIO_SND_CTL_TYPE_BYTES,
	VIRTIO_SND_CTL_TYPE_IEC958
};

enum {
	VIRTIO_SND_CTL_ACCESS_READ = 0,
	VIRTIO_SND_CTL_ACCESS_WRITE,
	VIRTIO_SND_CTL_ACCESS_VOLATILE,
	VIRTIO_SND_CTL_ACCESS_INACTIVE,
	VIRTIO_SND_CTL_ACCESS_TLV_READ,
	VIRTIO_SND_CTL_ACCESS_TLV_WRITE,
	VIRTIO_SND_CTL_ACCESS_TLV_COMMAND
};

struct virtio_snd_ctl_info {
	struct virtio_snd_info hdr;
	uint32_t role;
	uint32_t type;
	uint32_t access;
	uint32_t count;
	uint32_t index;
	uint8_t name[44];
	union {
		struct {
			uint32_t min;
			uint32_t max;
			uint32_t step;
		} integer;
		struct {
			uint64_t min;
			uint64_t max;
			uint64_t step;
		} integer64;
		struct {
			uint32_t items;
		} enumerated;
	} value;
};

struct virtio_snd_ctl_enum_item {
	uint8_t item[64];
};

struct virtio_snd_ctl_iec958 {
	uint8_t status[24];
	uint8_t subcode[147];
	uint8_t pad;
	uint8_t dig_subframe[4];
};

struct virtio_snd_ctl_value {
	union {
		uint32_t integer[128];
		uint64_t integer64[64];
		uint32_t enumerated[128];
		uint8_t bytes[512];
		struct virtio_snd_ctl_iec958 iec958;
	} value;
};

enum {
	VIRTIO_SND_CTL_EVT_MASK_VALUE = 0,
	VIRTIO_SND_CTL_EVT_MASK_INFO,
	VIRTIO_SND_CTL_EVT_MASK_TLV
};

struct virtio_snd_ctl_event {
	struct virtio_snd_hdr hdr;
	uint16_t control_id;
	uint16_t mask;
};

#endif /* VIRTIO_SND_IF_H */
