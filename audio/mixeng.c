#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "qemu/error-report.h"
#include "audio.h"

#define AUDIO_CAP "mixeng"
#include "audio_int.h"

#define ENDIAN_CONVERSION natural
#define ENDIAN_CONVERT(v) (v)

#define BSIZE 8
#define ITYPE int
#define IN_MIN SCHAR_MIN
#define IN_MAX SCHAR_MAX
#define SIGNED
#define SHIFT 8
#include "mixeng_template.h"
#undef SIGNED
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

#define BSIZE 8
#define ITYPE uint
#define IN_MIN 0
#define IN_MAX UCHAR_MAX
#define SHIFT 8
#include "mixeng_template.h"
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION

#define BSIZE 16
#define ITYPE int
#define IN_MIN SHRT_MIN
#define IN_MAX SHRT_MAX
#define SIGNED
#define SHIFT 16
#define ENDIAN_CONVERSION natural
#define ENDIAN_CONVERT(v) (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#define ENDIAN_CONVERSION swap
#define ENDIAN_CONVERT(v) bswap16 (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#undef SIGNED
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

#define BSIZE 16
#define ITYPE uint
#define IN_MIN 0
#define IN_MAX USHRT_MAX
#define SHIFT 16
#define ENDIAN_CONVERSION natural
#define ENDIAN_CONVERT(v) (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#define ENDIAN_CONVERSION swap
#define ENDIAN_CONVERT(v) bswap16 (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

#define BSIZE 32
#define ITYPE int
#define IN_MIN INT32_MIN
#define IN_MAX INT32_MAX
#define SIGNED
#define SHIFT 32
#define ENDIAN_CONVERSION natural
#define ENDIAN_CONVERT(v) (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#define ENDIAN_CONVERSION swap
#define ENDIAN_CONVERT(v) bswap32 (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#undef SIGNED
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

#define BSIZE 32
#define ITYPE uint
#define IN_MIN 0
#define IN_MAX UINT32_MAX
#define SHIFT 32
#define ENDIAN_CONVERSION natural
#define ENDIAN_CONVERT(v) (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#define ENDIAN_CONVERSION swap
#define ENDIAN_CONVERT(v) bswap32 (v)
#include "mixeng_template.h"
#undef ENDIAN_CONVERT
#undef ENDIAN_CONVERSION
#undef IN_MAX
#undef IN_MIN
#undef BSIZE
#undef ITYPE
#undef SHIFT

t_sample *mixeng_conv[2][2][2][3] = {
    {
        {
            {
                conv_natural_uint8_t_to_mono,
                conv_natural_uint16_t_to_mono,
                conv_natural_uint32_t_to_mono
            },
            {
                conv_natural_uint8_t_to_mono,
                conv_swap_uint16_t_to_mono,
                conv_swap_uint32_t_to_mono,
            }
        },
        {
            {
                conv_natural_int8_t_to_mono,
                conv_natural_int16_t_to_mono,
                conv_natural_int32_t_to_mono
            },
            {
                conv_natural_int8_t_to_mono,
                conv_swap_int16_t_to_mono,
                conv_swap_int32_t_to_mono
            }
        }
    },
    {
        {
            {
                conv_natural_uint8_t_to_stereo,
                conv_natural_uint16_t_to_stereo,
                conv_natural_uint32_t_to_stereo
            },
            {
                conv_natural_uint8_t_to_stereo,
                conv_swap_uint16_t_to_stereo,
                conv_swap_uint32_t_to_stereo
            }
        },
        {
            {
                conv_natural_int8_t_to_stereo,
                conv_natural_int16_t_to_stereo,
                conv_natural_int32_t_to_stereo
            },
            {
                conv_natural_int8_t_to_stereo,
                conv_swap_int16_t_to_stereo,
                conv_swap_int32_t_to_stereo,
            }
        }
    }
};

f_sample *mixeng_clip[2][2][2][3] = {
    {
        {
            {
                clip_natural_uint8_t_from_mono,
                clip_natural_uint16_t_from_mono,
                clip_natural_uint32_t_from_mono
            },
            {
                clip_natural_uint8_t_from_mono,
                clip_swap_uint16_t_from_mono,
                clip_swap_uint32_t_from_mono
            }
        },
        {
            {
                clip_natural_int8_t_from_mono,
                clip_natural_int16_t_from_mono,
                clip_natural_int32_t_from_mono
            },
            {
                clip_natural_int8_t_from_mono,
                clip_swap_int16_t_from_mono,
                clip_swap_int32_t_from_mono
            }
        }
    },
    {
        {
            {
                clip_natural_uint8_t_from_stereo,
                clip_natural_uint16_t_from_stereo,
                clip_natural_uint32_t_from_stereo
            },
            {
                clip_natural_uint8_t_from_stereo,
                clip_swap_uint16_t_from_stereo,
                clip_swap_uint32_t_from_stereo
            }
        },
        {
            {
                clip_natural_int8_t_from_stereo,
                clip_natural_int16_t_from_stereo,
                clip_natural_int32_t_from_stereo
            },
            {
                clip_natural_int8_t_from_stereo,
                clip_swap_int16_t_from_stereo,
                clip_swap_int32_t_from_stereo
            }
        }
    }
};

#ifdef FLOAT_MIXENG
#define CONV_NATURAL_FLOAT(x) (x)
#define CLIP_NATURAL_FLOAT(x) (x)
#else
static const float float_scale = (int64_t)INT32_MAX + 1;
#define CONV_NATURAL_FLOAT(x) ((x) * float_scale)

#ifdef RECIPROCAL
static const float float_scale_reciprocal = 1.f / ((int64_t)INT32_MAX + 1);
#define CLIP_NATURAL_FLOAT(x) ((x) * float_scale_reciprocal)
#else
#define CLIP_NATURAL_FLOAT(x) ((x) / float_scale)
#endif
#endif

static void conv_natural_float_to_mono(struct st_sample *dst, const void *src,
                                       int samples)
{
    float *in = (float *)src;

    while (samples--) {
        dst->r = dst->l = CONV_NATURAL_FLOAT(*in++);
        dst++;
    }
}

static void conv_natural_float_to_stereo(struct st_sample *dst, const void *src,
                                         int samples)
{
    float *in = (float *)src;

    while (samples--) {
        dst->l = CONV_NATURAL_FLOAT(*in++);
        dst->r = CONV_NATURAL_FLOAT(*in++);
        dst++;
    }
}

t_sample *mixeng_conv_float[2] = {
    conv_natural_float_to_mono,
    conv_natural_float_to_stereo,
};

static void clip_natural_float_from_mono(void *dst, const struct st_sample *src,
                                         int samples)
{
    float *out = (float *)dst;

    while (samples--) {
        *out++ = CLIP_NATURAL_FLOAT(src->l + src->r);
        src++;
    }
}

static void clip_natural_float_from_stereo(
    void *dst, const struct st_sample *src, int samples)
{
    float *out = (float *)dst;

    while (samples--) {
        *out++ = CLIP_NATURAL_FLOAT(src->l);
        *out++ = CLIP_NATURAL_FLOAT(src->r);
        src++;
    }
}

f_sample *mixeng_clip_float[2] = {
    clip_natural_float_from_mono,
    clip_natural_float_from_stereo,
};

void audio_sample_to_uint64(const void *samples, int pos,
                            uint64_t *left, uint64_t *right)
{
#ifdef FLOAT_MIXENG
    error_report(
        "Coreaudio and floating point samples are not supported by replay yet");
    abort();
#else
    const struct st_sample *sample = samples;
    sample += pos;
    *left = sample->l;
    *right = sample->r;
#endif
}

void audio_sample_from_uint64(void *samples, int pos,
                            uint64_t left, uint64_t right)
{
#ifdef FLOAT_MIXENG
    error_report(
        "Coreaudio and floating point samples are not supported by replay yet");
    abort();
#else
    struct st_sample *sample = samples;
    sample += pos;
    sample->l = left;
    sample->r = right;
#endif
}



struct rate {
    uint64_t opos;
    uint64_t opos_inc;
    uint32_t ipos;              /* position in the input stream (integer) */
    struct st_sample ilast;          /* last sample in the input stream */
};

void *st_rate_start (int inrate, int outrate)
{
    struct rate *rate = g_new0(struct rate, 1);

    rate->opos = 0;

    rate->opos_inc = ((uint64_t) inrate << 32) / outrate;

    rate->ipos = 0;
    rate->ilast.l = 0;
    rate->ilast.r = 0;
    return rate;
}

#define NAME st_rate_flow_mix
#define OP(a, b) a += b
#include "rate_template.h"

#define NAME st_rate_flow
#define OP(a, b) a = b
#include "rate_template.h"

void st_rate_stop (void *opaque)
{
    g_free (opaque);
}

uint32_t st_rate_frames_out(void *opaque, uint32_t frames_in)
{
    struct rate *rate = opaque;
    uint64_t opos_end, opos_delta;
    uint32_t ipos_end;
    uint32_t frames_out;

    if (rate->opos_inc == 1ULL << 32) {
        return frames_in;
    }

    if (!frames_in) {
        return 0;
    }

    ipos_end = rate->ipos - 1 + frames_in;
    opos_end = (uint64_t)ipos_end << 32;

    if (opos_end + rate->opos_inc <= rate->opos) {
        return 0;
    }
    opos_delta = opos_end - rate->opos + rate->opos_inc;
    frames_out = opos_delta / rate->opos_inc;

    return opos_delta % rate->opos_inc ? frames_out : frames_out - 1;
}

uint32_t st_rate_frames_in(void *opaque, uint32_t frames_out)
{
    struct rate *rate = opaque;
    uint64_t opos_start, opos_end;
    uint32_t ipos_start, ipos_end;

    if (rate->opos_inc == 1ULL << 32) {
        return frames_out;
    }

    if (frames_out) {
        opos_start = rate->opos;
        ipos_start = rate->ipos;
    } else {
        uint64_t offset;

        offset = (rate->opos_inc + (1ULL << 32) - 1) & ~((1ULL << 32) - 1);
        opos_start = rate->opos + offset;
        ipos_start = rate->ipos + (offset >> 32);
    }
    opos_end = opos_start - rate->opos_inc + rate->opos_inc * frames_out;
    ipos_end = (opos_end >> 32) + 1;

    return ipos_end + 1 > ipos_start ? ipos_end + 1 - ipos_start : 0;
}

void mixeng_clear (struct st_sample *buf, int len)
{
    memset (buf, 0, len * sizeof (struct st_sample));
}

void mixeng_volume (struct st_sample *buf, int len, struct mixeng_volume *vol)
{
    if (vol->mute) {
        mixeng_clear (buf, len);
        return;
    }

    while (len--) {
#ifdef FLOAT_MIXENG
        buf->l = buf->l * vol->l;
        buf->r = buf->r * vol->r;
#else
        buf->l = (buf->l * vol->l) >> 32;
        buf->r = (buf->r * vol->r) >> 32;
#endif
        buf += 1;
    }
}
