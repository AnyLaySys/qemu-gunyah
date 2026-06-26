#ifndef BSWAP_H
#define BSWAP_H

#undef  bswap16
#define bswap16(_x) __builtin_bswap16(_x)
#undef  bswap32
#define bswap32(_x) __builtin_bswap32(_x)
#undef  bswap64
#define bswap64(_x) __builtin_bswap64(_x)

static inline uint32_t bswap24(uint32_t x)
{
    return (((x & 0x000000ffU) << 16) |
            ((x & 0x0000ff00U) <<  0) |
            ((x & 0x00ff0000U) >> 16));
}

static inline void bswap16s(uint16_t *s)
{
    *s = __builtin_bswap16(*s);
}

static inline void bswap24s(uint32_t *s)
{
    *s = bswap24(*s & 0x00ffffffU);
}

static inline void bswap32s(uint32_t *s)
{
    *s = __builtin_bswap32(*s);
}

static inline void bswap64s(uint64_t *s)
{
    *s = __builtin_bswap64(*s);
}

#if HOST_BIG_ENDIAN
#define be_bswap(v, size) (v)
#define le_bswap(v, size) glue(__builtin_bswap, size)(v)
#define be_bswap24(v) (v)
#define le_bswap24(v) bswap24(v)
#define be_bswaps(v, size)
#define le_bswaps(p, size) \
            do { *p = glue(__builtin_bswap, size)(*p); } while (0)
#else
#define le_bswap(v, size) (v)
#define be_bswap24(v) bswap24(v)
#define le_bswap24(v) (v)
#define be_bswap(v, size) glue(__builtin_bswap, size)(v)
#define le_bswaps(v, size)
#define be_bswaps(p, size) \
            do { *p = glue(__builtin_bswap, size)(*p); } while (0)
#endif


#define CPU_CONVERT(endian, size, type)\
static inline type endian ## size ## _to_cpu(type v)\
{\
    return glue(endian, _bswap)(v, size);\
}\
\
static inline type cpu_to_ ## endian ## size(type v)\
{\
    return glue(endian, _bswap)(v, size);\
}\
\
static inline void endian ## size ## _to_cpus(type *p)\
{\
    glue(endian, _bswaps)(p, size);\
}\
\
static inline void cpu_to_ ## endian ## size ## s(type *p)\
{\
    glue(endian, _bswaps)(p, size);\
}

CPU_CONVERT(be, 16, uint16_t)
CPU_CONVERT(be, 32, uint32_t)
CPU_CONVERT(be, 64, uint64_t)

CPU_CONVERT(le, 16, uint16_t)
CPU_CONVERT(le, 32, uint32_t)
CPU_CONVERT(le, 64, uint64_t)

#undef CPU_CONVERT

#if HOST_BIG_ENDIAN
# define const_le64(_x)                          \
    ((((_x) & 0x00000000000000ffULL) << 56) |    \
     (((_x) & 0x000000000000ff00ULL) << 40) |    \
     (((_x) & 0x0000000000ff0000ULL) << 24) |    \
     (((_x) & 0x00000000ff000000ULL) <<  8) |    \
     (((_x) & 0x000000ff00000000ULL) >>  8) |    \
     (((_x) & 0x0000ff0000000000ULL) >> 24) |    \
     (((_x) & 0x00ff000000000000ULL) >> 40) |    \
     (((_x) & 0xff00000000000000ULL) >> 56))
# define const_le32(_x)                          \
    ((((_x) & 0x000000ffU) << 24) |              \
     (((_x) & 0x0000ff00U) <<  8) |              \
     (((_x) & 0x00ff0000U) >>  8) |              \
     (((_x) & 0xff000000U) >> 24))
# define const_le16(_x)                          \
    ((((_x) & 0x00ff) << 8) |                    \
     (((_x) & 0xff00) >> 8))
#else
# define const_le64(_x) (_x)
# define const_le32(_x) (_x)
# define const_le16(_x) (_x)
#endif



static inline int ldub_p(const void *ptr)
{
    return *(uint8_t *)ptr;
}

static inline int ldsb_p(const void *ptr)
{
    return *(int8_t *)ptr;
}

static inline void stb_p(void *ptr, uint8_t v)
{
    *(uint8_t *)ptr = v;
}


static inline int lduw_he_p(const void *ptr)
{
    uint16_t r;
    __builtin_memcpy(&r, ptr, sizeof(r));
    return r;
}

static inline int ldsw_he_p(const void *ptr)
{
    int16_t r;
    __builtin_memcpy(&r, ptr, sizeof(r));
    return r;
}

static inline void stw_he_p(void *ptr, uint16_t v)
{
    __builtin_memcpy(ptr, &v, sizeof(v));
}

static inline void st24_he_p(void *ptr, uint32_t v)
{
    __builtin_memcpy(ptr, &v, 3);
}

static inline int ldl_he_p(const void *ptr)
{
    int32_t r;
    __builtin_memcpy(&r, ptr, sizeof(r));
    return r;
}

static inline void stl_he_p(void *ptr, uint32_t v)
{
    __builtin_memcpy(ptr, &v, sizeof(v));
}

static inline uint64_t ldq_he_p(const void *ptr)
{
    uint64_t r;
    __builtin_memcpy(&r, ptr, sizeof(r));
    return r;
}

static inline void stq_he_p(void *ptr, uint64_t v)
{
    __builtin_memcpy(ptr, &v, sizeof(v));
}

static inline int lduw_le_p(const void *ptr)
{
    return (uint16_t)le_bswap(lduw_he_p(ptr), 16);
}

static inline int ldsw_le_p(const void *ptr)
{
    return (int16_t)le_bswap(lduw_he_p(ptr), 16);
}

static inline int ldl_le_p(const void *ptr)
{
    return le_bswap(ldl_he_p(ptr), 32);
}

static inline uint64_t ldq_le_p(const void *ptr)
{
    return le_bswap(ldq_he_p(ptr), 64);
}

static inline void stw_le_p(void *ptr, uint16_t v)
{
    stw_he_p(ptr, le_bswap(v, 16));
}

static inline void st24_le_p(void *ptr, uint32_t v)
{
    st24_he_p(ptr, le_bswap24(v));
}

static inline void stl_le_p(void *ptr, uint32_t v)
{
    stl_he_p(ptr, le_bswap(v, 32));
}

static inline void stq_le_p(void *ptr, uint64_t v)
{
    stq_he_p(ptr, le_bswap(v, 64));
}

static inline int lduw_be_p(const void *ptr)
{
    return (uint16_t)be_bswap(lduw_he_p(ptr), 16);
}

static inline int ldsw_be_p(const void *ptr)
{
    return (int16_t)be_bswap(lduw_he_p(ptr), 16);
}

static inline int ldl_be_p(const void *ptr)
{
    return be_bswap(ldl_he_p(ptr), 32);
}

static inline uint64_t ldq_be_p(const void *ptr)
{
    return be_bswap(ldq_he_p(ptr), 64);
}

static inline void stw_be_p(void *ptr, uint16_t v)
{
    stw_he_p(ptr, be_bswap(v, 16));
}

static inline void st24_be_p(void *ptr, uint32_t v)
{
    st24_he_p(ptr, be_bswap24(v));
}

static inline void stl_be_p(void *ptr, uint32_t v)
{
    stl_he_p(ptr, be_bswap(v, 32));
}

static inline void stq_be_p(void *ptr, uint64_t v)
{
    stq_he_p(ptr, be_bswap(v, 64));
}

static inline unsigned long leul_to_cpu(unsigned long v)
{
#if HOST_LONG_BITS == 32
    return le_bswap(v, 32);
#elif HOST_LONG_BITS == 64
    return le_bswap(v, 64);
#else
# error Unknown sizeof long
#endif
}

#define DO_STN_LDN_P(END) \
    static inline void stn_## END ## _p(void *ptr, int sz, uint64_t v)  \
    {                                                                   \
        switch (sz) {                                                   \
        case 1:                                                         \
            stb_p(ptr, v);                                              \
            break;                                                      \
        case 2:                                                         \
            stw_ ## END ## _p(ptr, v);                                  \
            break;                                                      \
        case 4:                                                         \
            stl_ ## END ## _p(ptr, v);                                  \
            break;                                                      \
        case 8:                                                         \
            stq_ ## END ## _p(ptr, v);                                  \
            break;                                                      \
        default:                                                        \
            g_assert_not_reached();                                     \
        }                                                               \
    }                                                                   \
    static inline uint64_t ldn_## END ## _p(const void *ptr, int sz)    \
    {                                                                   \
        switch (sz) {                                                   \
        case 1:                                                         \
            return ldub_p(ptr);                                         \
        case 2:                                                         \
            return lduw_ ## END ## _p(ptr);                             \
        case 4:                                                         \
            return (uint32_t)ldl_ ## END ## _p(ptr);                    \
        case 8:                                                         \
            return ldq_ ## END ## _p(ptr);                              \
        default:                                                        \
            g_assert_not_reached();                                     \
        }                                                               \
    }

DO_STN_LDN_P(he)
DO_STN_LDN_P(le)
DO_STN_LDN_P(be)

#undef DO_STN_LDN_P

#undef le_bswap
#undef be_bswap
#undef le_bswaps
#undef be_bswaps

#endif /* BSWAP_H */
