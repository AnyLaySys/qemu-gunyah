
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "audio.h"

#define AUDIO_CAP "aaudio"
#include "audio_int.h"

#include <aaudio/AAudio.h>

typedef struct AAudioVoiceOut {
    HWVoiceOut hw;
    AAudioStream *stream;
} AAudioVoiceOut;

typedef struct AAudioVoiceIn {
    HWVoiceIn hw;
    AAudioStream *stream;
} AAudioVoiceIn;

static aaudio_format_t qemu_to_aaudio_fmt(AudioFormat fmt)
{
    switch (fmt) {
    case AUDIO_FORMAT_S16:
        return AAUDIO_FORMAT_PCM_I16;
    case AUDIO_FORMAT_F32:
        return AAUDIO_FORMAT_PCM_FLOAT;
    default:
        dolog("Unsupported audio format %d, falling back to S16\n", fmt);
        return AAUDIO_FORMAT_PCM_I16;
    }
}

static AAudioStream *aaudio_open_stream(struct audsettings *as,
                                        aaudio_direction_t direction)
{
    AAudioStreamBuilder *builder = NULL;
    AAudioStream *stream = NULL;
    aaudio_result_t res;

    res = AAudio_createStreamBuilder(&builder);
    if (res != AAUDIO_OK) {
        dolog("AAudio_createStreamBuilder failed: %d\n", res);
        return NULL;
    }

    AAudioStreamBuilder_setDirection(builder, direction);
    AAudioStreamBuilder_setSampleRate(builder, as->freq);
    AAudioStreamBuilder_setChannelCount(builder, as->nchannels);
    AAudioStreamBuilder_setFormat(builder, qemu_to_aaudio_fmt(as->fmt));
    AAudioStreamBuilder_setPerformanceMode(builder,
                                           AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setBufferCapacityInFrames(builder, 1024 * 2);

    res = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK) {
        dolog("AAudioStreamBuilder_openStream failed: %d\n", res);
        return NULL;
    }

    return stream;
}


static int aaudio_init_out(HWVoiceOut *hw, struct audsettings *as,
                           void *drv_opaque)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;

    as->fmt = AUDIO_FORMAT_S16;

    aa->stream = aaudio_open_stream(as, AAUDIO_DIRECTION_OUTPUT);
    if (!aa->stream) {
        return -1;
    }

    audio_pcm_init_info(&hw->info, as);
    hw->samples = AAudioStream_getBufferSizeInFrames(aa->stream);

    return 0;
}

static void aaudio_fini_out(HWVoiceOut *hw)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;

    if (aa->stream) {
        AAudioStream_requestStop(aa->stream);
        AAudioStream_close(aa->stream);
        aa->stream = NULL;
    }
}

static size_t aaudio_write(HWVoiceOut *hw, void *buf, size_t len)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;
    int frames = len / hw->info.bytes_per_frame;
    aaudio_result_t written;

    if (!aa->stream) {
        return 0;
    }

    written = AAudioStream_write(aa->stream, buf, frames, 0);
    if (written < 0) {
        dolog("AAudioStream_write failed: %d\n", written);
        return 0;
    }

    return written * hw->info.bytes_per_frame;
}

static void aaudio_enable_out(HWVoiceOut *hw, bool enable)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;

    if (!aa->stream) {
        return;
    }

    if (enable) {
        AAudioStream_requestStart(aa->stream);
    } else {
        AAudioStream_requestPause(aa->stream);
    }
}


static int aaudio_init_in(HWVoiceIn *hw, struct audsettings *as,
                          void *drv_opaque)
{
    AAudioVoiceIn *aa = (AAudioVoiceIn *)hw;

    as->fmt = AUDIO_FORMAT_S16;

    aa->stream = aaudio_open_stream(as, AAUDIO_DIRECTION_INPUT);
    if (!aa->stream) {
        return -1;
    }

    audio_pcm_init_info(&hw->info, as);
    hw->samples = AAudioStream_getBufferSizeInFrames(aa->stream);

    return 0;
}

static void aaudio_fini_in(HWVoiceIn *hw)
{
    AAudioVoiceIn *aa = (AAudioVoiceIn *)hw;

    if (aa->stream) {
        AAudioStream_requestStop(aa->stream);
        AAudioStream_close(aa->stream);
        aa->stream = NULL;
    }
}

static size_t aaudio_read(HWVoiceIn *hw, void *buf, size_t len)
{
    AAudioVoiceIn *aa = (AAudioVoiceIn *)hw;
    int frames = len / hw->info.bytes_per_frame;
    aaudio_result_t nread;

    if (!aa->stream) {
        return 0;
    }

    nread = AAudioStream_read(aa->stream, buf, frames, 0);
    if (nread < 0) {
        dolog("AAudioStream_read failed: %d\n", nread);
        return 0;
    }

    return nread * hw->info.bytes_per_frame;
}

static void aaudio_enable_in(HWVoiceIn *hw, bool enable)
{
    AAudioVoiceIn *aa = (AAudioVoiceIn *)hw;

    if (!aa->stream) {
        return;
    }

    if (enable) {
        AAudioStream_requestStart(aa->stream);
    } else {
        AAudioStream_requestStop(aa->stream);
    }
}


static void *aaudio_audio_init(Audiodev *dev, Error **errp)
{
    return &aaudio_audio_init; /* non-NULL = success */
}

static void aaudio_audio_fini(void *opaque)
{
    (void)opaque;
}

static struct audio_pcm_ops aaudio_pcm_ops = {
    .init_out       = aaudio_init_out,
    .fini_out       = aaudio_fini_out,
    .write          = aaudio_write,
    .buffer_get_free = audio_generic_buffer_get_free,
    .run_buffer_out = audio_generic_run_buffer_out,
    .enable_out     = aaudio_enable_out,

    .init_in        = aaudio_init_in,
    .fini_in        = aaudio_fini_in,
    .read           = aaudio_read,
    .run_buffer_in  = audio_generic_run_buffer_in,
    .enable_in      = aaudio_enable_in,
};

static struct audio_driver aaudio_audio_driver = {
    .name           = "aaudio",
    .descr          = "Android AAudio audio",
    .init           = aaudio_audio_init,
    .fini           = aaudio_audio_fini,
    .pcm_ops        = &aaudio_pcm_ops,
    .max_voices_out = 1,
    .max_voices_in  = 1,
    .voice_size_out = sizeof(AAudioVoiceOut),
    .voice_size_in  = sizeof(AAudioVoiceIn),
};

static void register_audio_aaudio(void)
{
    audio_driver_register(&aaudio_audio_driver);
}
type_init(register_audio_aaudio);
