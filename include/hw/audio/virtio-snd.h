
#ifndef QEMU_VIRTIO_SOUND_H
#define QEMU_VIRTIO_SOUND_H

#include "hw/virtio/virtio.h"
#include "audio/audio.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_snd.h"

#define TYPE_VIRTIO_SND "virtio-sound-device"
#define VIRTIO_SND(obj) \
        OBJECT_CHECK(VirtIOSound, (obj), TYPE_VIRTIO_SND)


typedef struct virtio_snd_config virtio_snd_config;


typedef struct virtio_snd_hdr virtio_snd_hdr;

typedef struct virtio_snd_event virtio_snd_event;

typedef struct virtio_snd_query_info virtio_snd_query_info;


typedef struct virtio_snd_jack_hdr virtio_snd_jack_hdr;

typedef struct virtio_snd_jack_info virtio_snd_jack_info;

typedef struct virtio_snd_jack_remap virtio_snd_jack_remap;

typedef struct virtio_snd_pcm_hdr virtio_snd_pcm_hdr;

typedef struct virtio_snd_pcm_info virtio_snd_pcm_info;

typedef struct virtio_snd_pcm_set_params virtio_snd_pcm_set_params;

typedef struct virtio_snd_pcm_xfer virtio_snd_pcm_xfer;

typedef struct virtio_snd_pcm_status virtio_snd_pcm_status;


typedef struct VirtIOSound VirtIOSound;

typedef struct VirtIOSoundPCMStream VirtIOSoundPCMStream;

typedef struct virtio_snd_ctrl_command virtio_snd_ctrl_command;

typedef struct VirtIOSoundPCM VirtIOSoundPCM;

typedef struct VirtIOSoundPCMBuffer VirtIOSoundPCMBuffer;

struct VirtIOSoundPCMBuffer {
    QSIMPLEQ_ENTRY(VirtIOSoundPCMBuffer) entry;
    VirtQueueElement *elem;
    VirtQueue *vq;
    size_t size;
    uint64_t offset;
    bool populated;
    uint8_t data[];
};

struct VirtIOSoundPCM {
    VirtIOSound *snd;
    virtio_snd_pcm_set_params *pcm_params;
    VirtIOSoundPCMStream **streams;
};

struct VirtIOSoundPCMStream {
    VirtIOSoundPCM *pcm;
    virtio_snd_pcm_info info;
    virtio_snd_pcm_set_params params;
    uint32_t id;
    uint8_t positions[VIRTIO_SND_CHMAP_MAX_SIZE];
    VirtIOSound *s;
    bool flushing;
    audsettings as;
    union {
        SWVoiceIn *in;
        SWVoiceOut *out;
    } voice;
    QemuMutex queue_mutex;
    bool active;
    QSIMPLEQ_HEAD(, VirtIOSoundPCMBuffer) queue;
};

struct VirtIOSound {
    VirtIODevice parent_obj;

    VirtQueue *queues[VIRTIO_SND_VQ_MAX];
    uint64_t features;
    VirtIOSoundPCM *pcm;
    QEMUSoundCard card;
    VMChangeStateEntry *vmstate;
    virtio_snd_config snd_conf;
    QemuMutex cmdq_mutex;
    QTAILQ_HEAD(, virtio_snd_ctrl_command) cmdq;
    bool processing_cmdq;
    QSIMPLEQ_HEAD(, VirtIOSoundPCMBuffer) invalid;
};

struct virtio_snd_ctrl_command {
    VirtQueueElement *elem;
    VirtQueue *vq;
    virtio_snd_hdr ctrl;
    virtio_snd_hdr resp;
    size_t payload_size;
    QTAILQ_ENTRY(virtio_snd_ctrl_command) next;
};
#endif
