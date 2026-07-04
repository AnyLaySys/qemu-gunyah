

#ifndef SIGNED
#define HALF (IN_MAX >> 1)
#endif

#define ET glue (ENDIAN_CONVERSION, glue (glue (glue (_, ITYPE), BSIZE), _t))
#define IN_T glue (glue (ITYPE, BSIZE), _t)

#ifdef FLOAT_MIXENG
static inline mixeng_real glue (conv_, ET) (IN_T v)
{
    IN_T nv = ENDIAN_CONVERT (v);

#ifdef RECIPROCAL
#ifdef SIGNED
    return nv * (2.f / ((mixeng_real)IN_MAX - IN_MIN));
#else
    return (nv - HALF) * (2.f / (mixeng_real)IN_MAX);
#endif
#else  /* !RECIPROCAL */
#ifdef SIGNED
    return nv / (((mixeng_real)IN_MAX - IN_MIN) / 2.f);
#else
    return (nv - HALF) / ((mixeng_real)IN_MAX / 2.f);
#endif
#endif
}

static inline IN_T glue (clip_, ET) (mixeng_real v)
{
    if (v >= 1.f) {
        return IN_MAX;
    } else if (v < -1.f) {
        return IN_MIN;
    }

#ifdef SIGNED
    return ENDIAN_CONVERT((IN_T)(v * (((mixeng_real)IN_MAX - IN_MIN) / 2.f)));
#else
    return ENDIAN_CONVERT((IN_T)((v * ((mixeng_real)IN_MAX / 2.f)) + HALF));
#endif
}

#else  /* !FLOAT_MIXENG */

static inline int64_t glue (conv_, ET) (IN_T v)
{
    IN_T nv = ENDIAN_CONVERT (v);
#ifdef SIGNED
    return ((int64_t) nv) << (32 - SHIFT);
#else
    return ((int64_t) nv - HALF) << (32 - SHIFT);
#endif
}

static inline IN_T glue (clip_, ET) (int64_t v)
{
    if (v >= 0x7fffffffLL) {
        return IN_MAX;
    } else if (v < -2147483648LL) {
        return IN_MIN;
    }

#ifdef SIGNED
    return ENDIAN_CONVERT ((IN_T) (v >> (32 - SHIFT)));
#else
    return ENDIAN_CONVERT ((IN_T) ((v >> (32 - SHIFT)) + HALF));
#endif
}
#endif

static void glue (glue (conv_, ET), _to_stereo)
    (struct st_sample *dst, const void *src, int samples)
{
    struct st_sample *out = dst;
    IN_T *in = (IN_T *) src;

    while (samples--) {
        out->l = glue (conv_, ET) (*in++);
        out->r = glue (conv_, ET) (*in++);
        out += 1;
    }
}

static void glue (glue (conv_, ET), _to_mono)
    (struct st_sample *dst, const void *src, int samples)
{
    struct st_sample *out = dst;
    IN_T *in = (IN_T *) src;

    while (samples--) {
        out->l = glue (conv_, ET) (in[0]);
        out->r = out->l;
        out += 1;
        in += 1;
    }
}

static void glue (glue (clip_, ET), _from_stereo)
    (void *dst, const struct st_sample *src, int samples)
{
    const struct st_sample *in = src;
    IN_T *out = (IN_T *) dst;
    while (samples--) {
        *out++ = glue (clip_, ET) (in->l);
        *out++ = glue (clip_, ET) (in->r);
        in += 1;
    }
}

static void glue (glue (clip_, ET), _from_mono)
    (void *dst, const struct st_sample *src, int samples)
{
    const struct st_sample *in = src;
    IN_T *out = (IN_T *) dst;
    while (samples--) {
        *out++ = glue (clip_, ET) (in->l + in->r);
        in += 1;
    }
}

#undef ET
#undef HALF
#undef IN_T
