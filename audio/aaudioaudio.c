
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "audio.h"

#define AUDIO_CAP "aaudio"
#include "audio_int.h"

#include <aaudio/AAudio.h>

typedef struct AAudioVoiceOut {
    HWVoiceOut hw;
    AAudioStream *stream;
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t read_pos;
    uint32_t write_pos;
} AAudioVoiceOut;

typedef struct AAudioVoiceIn {
    HWVoiceIn hw;
    AAudioStream *stream;
} AAudioVoiceIn;

static int input_fd = -1;

void aaudio_set_input_fd(int fd)
{
    int flags = fd < 0 ? -1 : fcntl(fd, F_GETFL);

    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    input_fd = fd;
}

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

static aaudio_data_callback_result_t aaudio_data_callback(
    AAudioStream *stream, void *user_data, void *audio_data, int32_t frames)
{
    AAudioVoiceOut *aa = user_data;
    uint8_t *out = audio_data;
    uint32_t read_pos = qatomic_read(&aa->read_pos);
    uint32_t write_pos = qatomic_load_acquire(&aa->write_pos);
    size_t size = (size_t)frames * aa->hw.info.bytes_per_frame;
    size_t available = MIN(size, (uint32_t)(write_pos - read_pos));
    size_t offset = read_pos % aa->buffer_size;
    size_t first = MIN(available, (size_t)aa->buffer_size - offset);

    (void)stream;
    if (available) {
        memcpy(out, aa->buffer + offset, first);
        memcpy(out + first, aa->buffer, available - first);
    }
    if (available < size) {
        memset(out + available, 0, size - available);
    }
    qatomic_store_release(&aa->read_pos, read_pos + available);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static AAudioStream *aaudio_open_stream(struct audsettings *as,
                                        aaudio_direction_t direction,
                                        void *user_data)
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
    AAudioStreamBuilder_setChannelCount(builder, as->nchannels);
    AAudioStreamBuilder_setFormat(builder, qemu_to_aaudio_fmt(as->fmt));
    AAudioStreamBuilder_setPerformanceMode(
        builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    if (direction == AAUDIO_DIRECTION_OUTPUT) {
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
        AAudioStreamBuilder_setDataCallback(builder, aaudio_data_callback, user_data);
    } else {
        AAudioStreamBuilder_setSampleRate(builder, as->freq);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setBufferCapacityInFrames(builder, 1024 * 2);
    }

    res = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK) {
        dolog("AAudioStreamBuilder_openStream failed: %d\n", res);
        return NULL;
    }

    if (direction == AAUDIO_DIRECTION_OUTPUT) {
        as->freq = AAudioStream_getSampleRate(stream);
        as->nchannels = AAudioStream_getChannelCount(stream);
    }

    return stream;
}


static int aaudio_init_out(HWVoiceOut *hw, struct audsettings *as,
                           void *drv_opaque)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;

    as->fmt = AUDIO_FORMAT_S16;

    aa->stream = aaudio_open_stream(as, AAUDIO_DIRECTION_OUTPUT, aa);
    if (!aa->stream) {
        return -1;
    }

    audio_pcm_init_info(&hw->info, as);
    hw->samples = AAudioStream_getBufferSizeInFrames(aa->stream);
    aa->buffer_size = hw->samples * hw->info.bytes_per_frame;
    aa->buffer = g_malloc(aa->buffer_size);

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
    g_free(aa->buffer);
    aa->buffer = NULL;
    aa->buffer_size = 0;
}

static size_t aaudio_write(HWVoiceOut *hw, void *buf, size_t len)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;
    uint32_t write_pos;
    uint32_t read_pos;
    size_t size;
    size_t offset;
    size_t first;

    if (!aa->stream || !aa->buffer) {
        return 0;
    }

    write_pos = qatomic_read(&aa->write_pos);
    read_pos = qatomic_load_acquire(&aa->read_pos);
    size = MIN(len, (size_t)(aa->buffer_size - (write_pos - read_pos)));
    size -= size % hw->info.bytes_per_frame;
    offset = write_pos % aa->buffer_size;
    first = MIN(size, (size_t)aa->buffer_size - offset);
    memcpy(aa->buffer + offset, buf, first);
    memcpy(aa->buffer, (uint8_t *)buf + first, size - first);
    qatomic_store_release(&aa->write_pos, write_pos + size);

    return size;
}

static size_t aaudio_buffer_get_free(HWVoiceOut *hw)
{
    AAudioVoiceOut *aa = (AAudioVoiceOut *)hw;
    uint32_t write_pos = qatomic_read(&aa->write_pos);
    uint32_t read_pos = qatomic_load_acquire(&aa->read_pos);

    return aa->buffer_size - (write_pos - read_pos);
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

    if (input_fd >= 0) {
        audio_pcm_init_info(&hw->info, as);
        hw->samples = 2048;
        return 0;
    }

    aa->stream = aaudio_open_stream(as, AAUDIO_DIRECTION_INPUT, NULL);
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

    if (input_fd >= 0) {
        ssize_t size = read(input_fd, buf, len);

        return size > 0 ? size : 0;
    }
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

    if (input_fd >= 0) {
        return;
    }
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
    .buffer_get_free = aaudio_buffer_get_free,
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
