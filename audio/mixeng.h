
#ifndef QEMU_MIXENG_H
#define QEMU_MIXENG_H

#ifdef FLOAT_MIXENG
typedef float mixeng_real;
struct mixeng_volume { int mute; mixeng_real r; mixeng_real l; };
struct st_sample { mixeng_real l; mixeng_real r; };
#else
struct mixeng_volume { int mute; int64_t r; int64_t l; };
struct st_sample { int64_t l; int64_t r; };
#endif
typedef struct st_sample st_sample;

typedef void (t_sample) (struct st_sample *dst, const void *src, int samples);
typedef void (f_sample) (void *dst, const struct st_sample *src, int samples);

extern t_sample *mixeng_conv[2][2][2][3];
extern f_sample *mixeng_clip[2][2][2][3];

extern t_sample *mixeng_conv_float[2];
extern f_sample *mixeng_clip_float[2];

void *st_rate_start (int inrate, int outrate);
void st_rate_flow(void *opaque, st_sample *ibuf, st_sample *obuf,
                  size_t *isamp, size_t *osamp);
void st_rate_flow_mix(void *opaque, st_sample *ibuf, st_sample *obuf,
                      size_t *isamp, size_t *osamp);
void st_rate_stop (void *opaque);
uint32_t st_rate_frames_out(void *opaque, uint32_t frames_in);
uint32_t st_rate_frames_in(void *opaque, uint32_t frames_out);
void mixeng_clear (struct st_sample *buf, int len);
void mixeng_volume (struct st_sample *buf, int len, struct mixeng_volume *vol);

#endif /* QEMU_MIXENG_H */
