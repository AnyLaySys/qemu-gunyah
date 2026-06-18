










#include "host/load-extract-al16-al8.h.inc"
#include "host/store-insert-al16.h.inc"

#ifdef CONFIG_ATOMIC64
# define HAVE_al8          true
#else
# define HAVE_al8          false
#endif
#define HAVE_al8_fast      (ATOMIC_REG_SIZE >= 8)








static int required_atomicity(CPUState *cpu, uintptr_t p, MemOp memop)
{
    MemOp atom = memop & MO_ATOM_MASK;
    MemOp size = memop & MO_SIZE;
    MemOp half = size ? size - 1 : 0;
    unsigned tmp;
    int atmax;

    switch (atom) {
    case MO_ATOM_NONE:
        atmax = MO_8;
        break;

    case MO_ATOM_IFALIGN_PAIR:
        size = half;


    case MO_ATOM_IFALIGN:
        tmp = (1 << size) - 1;
        atmax = p & tmp ? MO_8 : size;
        break;

    case MO_ATOM_WITHIN16:
        tmp = p & 15;
        atmax = (tmp + (1 << size) <= 16 ? size : MO_8);
        break;

    case MO_ATOM_WITHIN16_PAIR:
        tmp = p & 15;
        if (tmp + (1 << size) <= 16) {
            atmax = size;
        } else if (tmp + (1 << half) == 16) {




            atmax = half;
        } else {




            atmax = -half;
        }
        break;

    case MO_ATOM_SUBALIGN:






        tmp = ctz32(p);
        atmax = MIN(size, tmp);
        break;

    default:
        g_assert_not_reached();
    }







    if (cpu_in_serial_context(cpu)) {
        return MO_8;
    }
    return atmax;
}







static inline uint16_t load_atomic2(void *pv)
{
    uint16_t *p = __builtin_assume_aligned(pv, 2);
    return qatomic_read(p);
}







static inline uint32_t load_atomic4(void *pv)
{
    uint32_t *p = __builtin_assume_aligned(pv, 4);
    return qatomic_read(p);
}







static inline uint64_t load_atomic8(void *pv)
{
    uint64_t *p = __builtin_assume_aligned(pv, 8);

    qemu_build_assert(HAVE_al8);
    return qatomic_read__nocheck(p);
}










static uint64_t load_atomic8_or_exit(CPUState *cpu, uintptr_t ra, void *pv)
{
    if (HAVE_al8) {
        return load_atomic8(pv);
    }

#ifdef CONFIG_USER_ONLY






    WITH_MMAP_LOCK_GUARD() {
        if (!page_check_range(h2g(pv), 8, PAGE_WRITE_ORG)) {
            uint64_t *p = __builtin_assume_aligned(pv, 8);
            return *p;
        }
    }
#endif


    trace_load_atom8_or_exit_fallback(ra);
    cpu_loop_exit_atomic(cpu, ra);
}










static Int128 load_atomic16_or_exit(CPUState *cpu, uintptr_t ra, void *pv)
{
    Int128 *p = __builtin_assume_aligned(pv, 16);

    if (HAVE_ATOMIC128_RO) {
        return atomic16_read_ro(p);
    }













    WITH_MMAP_LOCK_GUARD() {
#ifdef CONFIG_USER_ONLY
        if (!page_check_range(h2g(p), 16, PAGE_WRITE_ORG)) {
            return *p;
        }
#endif
        if (HAVE_ATOMIC128_RW) {
            return atomic16_read_rw(p);
        }
    }


    trace_load_atom16_or_exit_fallback(ra);
    cpu_loop_exit_atomic(cpu, ra);
}







static uint32_t load_atom_extract_al4x2(void *pv)
{
    uintptr_t pi = (uintptr_t)pv;
    int sh = (pi & 3) * 8;
    uint32_t a, b;

    pv = (void *)(pi & ~3);
    a = load_atomic4(pv);
    b = load_atomic4(pv + 4);

    if (HOST_BIG_ENDIAN) {
        return (a << sh) | (b >> (-sh & 31));
    } else {
        return (a >> sh) | (b << (-sh & 31));
    }
}







static uint64_t load_atom_extract_al8x2(void *pv)
{
    uintptr_t pi = (uintptr_t)pv;
    int sh = (pi & 7) * 8;
    uint64_t a, b;

    pv = (void *)(pi & ~7);
    a = load_atomic8(pv);
    b = load_atomic8(pv + 8);

    if (HOST_BIG_ENDIAN) {
        return (a << sh) | (b >> (-sh & 63));
    } else {
        return (a >> sh) | (b << (-sh & 63));
    }
}













static uint32_t load_atom_extract_al8_or_exit(CPUState *cpu, uintptr_t ra,
                                              void *pv, int s)
{
    uintptr_t pi = (uintptr_t)pv;
    int o = pi & 7;
    int shr = (HOST_BIG_ENDIAN ? 8 - s - o : o) * 8;

    pv = (void *)(pi & ~7);
    return load_atomic8_or_exit(cpu, ra, pv) >> shr;
}
















static uint64_t load_atom_extract_al16_or_exit(CPUState *cpu, uintptr_t ra,
                                               void *pv, int s)
{
    uintptr_t pi = (uintptr_t)pv;
    int o = pi & 7;
    int shr = (HOST_BIG_ENDIAN ? 16 - s - o : o) * 8;
    Int128 r;





    pv = (void *)(pi & ~7);
    r = load_atomic16_or_exit(cpu, ra, pv);

    r = int128_urshift(r, shr);
    return int128_getlo(r);
}







static inline uint32_t load_atom_4_by_2(void *pv)
{
    uint32_t a = load_atomic2(pv);
    uint32_t b = load_atomic2(pv + 2);

    if (HOST_BIG_ENDIAN) {
        return (a << 16) | b;
    } else {
        return (b << 16) | a;
    }
}







static inline uint64_t load_atom_8_by_2(void *pv)
{
    uint32_t a = load_atom_4_by_2(pv);
    uint32_t b = load_atom_4_by_2(pv + 4);

    if (HOST_BIG_ENDIAN) {
        return ((uint64_t)a << 32) | b;
    } else {
        return ((uint64_t)b << 32) | a;
    }
}







static inline uint64_t load_atom_8_by_4(void *pv)
{
    uint32_t a = load_atomic4(pv);
    uint32_t b = load_atomic4(pv + 4);

    if (HOST_BIG_ENDIAN) {
        return ((uint64_t)a << 32) | b;
    } else {
        return ((uint64_t)b << 32) | a;
    }
}







static inline uint64_t load_atom_8_by_8_or_4(void *pv)
{
    if (HAVE_al8_fast) {
        return load_atomic8(pv);
    } else {
        return load_atom_8_by_4(pv);
    }
}








static uint16_t load_atom_2(CPUState *cpu, uintptr_t ra,
                            void *pv, MemOp memop)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;

    if (likely((pi & 1) == 0)) {
        return load_atomic2(pv);
    }
    if (HAVE_ATOMIC128_RO) {
        intptr_t left_in_page = -(pi | TARGET_PAGE_MASK);
        if (likely(left_in_page > 8)) {
            return load_atom_extract_al16_or_al8(pv, 2);
        }
    }

    atmax = required_atomicity(cpu, pi, memop);
    switch (atmax) {
    case MO_8:
        return lduw_he_p(pv);
    case MO_16:

        if (!HAVE_al8_fast && (pi & 3) == 1) {

            return load_atomic4(pv - 1) >> 8;
        }
        if ((pi & 15) != 7) {
            return load_atom_extract_al8_or_exit(cpu, ra, pv, 2);
        }
        return load_atom_extract_al16_or_exit(cpu, ra, pv, 2);
    default:
        g_assert_not_reached();
    }
}








static uint32_t load_atom_4(CPUState *cpu, uintptr_t ra,
                            void *pv, MemOp memop)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;

    if (likely((pi & 3) == 0)) {
        return load_atomic4(pv);
    }
    if (HAVE_ATOMIC128_RO) {
        intptr_t left_in_page = -(pi | TARGET_PAGE_MASK);
        if (likely(left_in_page > 8)) {
            return load_atom_extract_al16_or_al8(pv, 4);
        }
    }

    atmax = required_atomicity(cpu, pi, memop);
    switch (atmax) {
    case MO_8:
    case MO_16:
    case -MO_16:






        return load_atom_extract_al4x2(pv);
    case MO_32:
        if (!(pi & 4)) {
            return load_atom_extract_al8_or_exit(cpu, ra, pv, 4);
        }
        return load_atom_extract_al16_or_exit(cpu, ra, pv, 4);
    default:
        g_assert_not_reached();
    }
}








static uint64_t load_atom_8(CPUState *cpu, uintptr_t ra,
                            void *pv, MemOp memop)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;





    if (HAVE_al8 && likely((pi & 7) == 0)) {
        return load_atomic8(pv);
    }
    if (HAVE_ATOMIC128_RO) {
        return load_atom_extract_al16_or_al8(pv, 8);
    }

    atmax = required_atomicity(cpu, pi, memop);
    if (atmax == MO_64) {
        if (!HAVE_al8 && (pi & 7) == 0) {
            load_atomic8_or_exit(cpu, ra, pv);
        }
        return load_atom_extract_al16_or_exit(cpu, ra, pv, 8);
    }
    if (HAVE_al8_fast) {
        return load_atom_extract_al8x2(pv);
    }
    switch (atmax) {
    case MO_8:
        return ldq_he_p(pv);
    case MO_16:
        return load_atom_8_by_2(pv);
    case MO_32:
        return load_atom_8_by_4(pv);
    case -MO_32:
        if (HAVE_al8) {
            return load_atom_extract_al8x2(pv);
        }
        trace_load_atom8_fallback(memop, ra);
        cpu_loop_exit_atomic(cpu, ra);
    default:
        g_assert_not_reached();
    }
}








static Int128 load_atom_16(CPUState *cpu, uintptr_t ra,
                           void *pv, MemOp memop)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;
    Int128 r;
    uint64_t a, b;





    if (HAVE_ATOMIC128_RO && likely((pi & 15) == 0)) {
        return atomic16_read_ro(pv);
    }

    atmax = required_atomicity(cpu, pi, memop);
    switch (atmax) {
    case MO_8:
        memcpy(&r, pv, 16);
        return r;
    case MO_16:
        a = load_atom_8_by_2(pv);
        b = load_atom_8_by_2(pv + 8);
        break;
    case MO_32:
        a = load_atom_8_by_4(pv);
        b = load_atom_8_by_4(pv + 8);
        break;
    case MO_64:
        if (!HAVE_al8) {
            trace_load_atom16_fallback(memop, ra);
            cpu_loop_exit_atomic(cpu, ra);
        }
        a = load_atomic8(pv);
        b = load_atomic8(pv + 8);
        break;
    case -MO_64:
        if (!HAVE_al8) {
            trace_load_atom16_fallback(memop, ra);
            cpu_loop_exit_atomic(cpu, ra);
        }
        a = load_atom_extract_al8x2(pv);
        b = load_atom_extract_al8x2(pv + 8);
        break;
    case MO_128:
        return load_atomic16_or_exit(cpu, ra, pv);
    default:
        g_assert_not_reached();
    }
    return int128_make128(HOST_BIG_ENDIAN ? b : a, HOST_BIG_ENDIAN ? a : b);
}








static inline void store_atomic2(void *pv, uint16_t val)
{
    uint16_t *p = __builtin_assume_aligned(pv, 2);
    qatomic_set(p, val);
}








static inline void store_atomic4(void *pv, uint32_t val)
{
    uint32_t *p = __builtin_assume_aligned(pv, 4);
    qatomic_set(p, val);
}








static inline void store_atomic8(void *pv, uint64_t val)
{
    uint64_t *p = __builtin_assume_aligned(pv, 8);

    qemu_build_assert(HAVE_al8);
    qatomic_set__nocheck(p, val);
}




static inline void store_atom_4_by_2(void *pv, uint32_t val)
{
    store_atomic2(pv, val >> (HOST_BIG_ENDIAN ? 16 : 0));
    store_atomic2(pv + 2, val >> (HOST_BIG_ENDIAN ? 0 : 16));
}




static inline void store_atom_8_by_2(void *pv, uint64_t val)
{
    store_atom_4_by_2(pv, val >> (HOST_BIG_ENDIAN ? 32 : 0));
    store_atom_4_by_2(pv + 4, val >> (HOST_BIG_ENDIAN ? 0 : 32));
}




static inline void store_atom_8_by_4(void *pv, uint64_t val)
{
    store_atomic4(pv, val >> (HOST_BIG_ENDIAN ? 32 : 0));
    store_atomic4(pv + 4, val >> (HOST_BIG_ENDIAN ? 0 : 32));
}









static void store_atom_insert_al4(uint32_t *p, uint32_t val, uint32_t msk)
{
    uint32_t old, new;

    p = __builtin_assume_aligned(p, 4);
    old = qatomic_read(p);
    do {
        new = (old & ~msk) | val;
    } while (!__atomic_compare_exchange_n(p, &old, new, true,
                                          __ATOMIC_RELAXED, __ATOMIC_RELAXED));
}









static void store_atom_insert_al8(uint64_t *p, uint64_t val, uint64_t msk)
{
    uint64_t old, new;

    qemu_build_assert(HAVE_al8);
    p = __builtin_assume_aligned(p, 8);
    old = qatomic_read__nocheck(p);
    do {
        new = (old & ~msk) | val;
    } while (!__atomic_compare_exchange_n(p, &old, new, true,
                                          __ATOMIC_RELAXED, __ATOMIC_RELAXED));
}










static uint64_t store_bytes_leN(void *pv, int size, uint64_t val_le)
{
    uint8_t *p = pv;
    for (int i = 0; i < size; i++, val_le >>= 8) {
        p[i] = val_le;
    }
    return val_le;
}









G_GNUC_UNUSED
static uint64_t store_parts_leN(void *pv, int size, uint64_t val_le)
{
    do {
        int n;


        switch (((uintptr_t)pv | size) & 7) {
        case 4:
            store_atomic4(pv, le32_to_cpu(val_le));
            val_le >>= 32;
            n = 4;
            break;
        case 2:
        case 6:
            store_atomic2(pv, le16_to_cpu(val_le));
            val_le >>= 16;
            n = 2;
            break;
        default:
            *(uint8_t *)pv = val_le;
            val_le >>= 8;
            n = 1;
            break;
        case 0:
            g_assert_not_reached();
        }
        pv += n;
        size -= n;
    } while (size != 0);

    return val_le;
}










static uint64_t store_whole_le4(void *pv, int size, uint64_t val_le)
{
    int sz = size * 8;
    int o = (uintptr_t)pv & 3;
    int sh = o * 8;
    uint32_t m = MAKE_64BIT_MASK(0, sz);
    uint32_t v;

    if (HOST_BIG_ENDIAN) {
        v = bswap32(val_le) >> sh;
        m = bswap32(m) >> sh;
    } else {
        v = val_le << sh;
        m <<= sh;
    }
    store_atom_insert_al4(pv - o, v, m);
    return val_le >> sz;
}










static uint64_t store_whole_le8(void *pv, int size, uint64_t val_le)
{
    int sz = size * 8;
    int o = (uintptr_t)pv & 7;
    int sh = o * 8;
    uint64_t m = MAKE_64BIT_MASK(0, sz);
    uint64_t v;

    qemu_build_assert(HAVE_al8);
    if (HOST_BIG_ENDIAN) {
        v = bswap64(val_le) >> sh;
        m = bswap64(m) >> sh;
    } else {
        v = val_le << sh;
        m <<= sh;
    }
    store_atom_insert_al8(pv - o, v, m);
    return val_le >> sz;
}










static uint64_t store_whole_le16(void *pv, int size, Int128 val_le)
{
    int sz = size * 8;
    int o = (uintptr_t)pv & 15;
    int sh = o * 8;
    Int128 m, v;

    qemu_build_assert(HAVE_CMPXCHG128);


    if (sz <= 64) {
        m = int128_make64(MAKE_64BIT_MASK(0, sz));
    } else {
        m = int128_make128(-1, MAKE_64BIT_MASK(0, sz - 64));
    }

    if (HOST_BIG_ENDIAN) {
        v = int128_urshift(bswap128(val_le), sh);
        m = int128_urshift(bswap128(m), sh);
    } else {
        v = int128_lshift(val_le, sh);
        m = int128_lshift(m, sh);
    }
    store_atom_insert_al16(pv - o, v, m);

    if (sz <= 64) {
        return 0;
    }
    return int128_gethi(val_le) >> (sz - 64);
}









static void store_atom_2(CPUState *cpu, uintptr_t ra,
                         void *pv, MemOp memop, uint16_t val)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;

    if (likely((pi & 1) == 0)) {
        store_atomic2(pv, val);
        return;
    }

    atmax = required_atomicity(cpu, pi, memop);
    if (atmax == MO_8) {
        stw_he_p(pv, val);
        return;
    }





    if ((pi & 3) == 1) {
        store_atom_insert_al4(pv - 1, (uint32_t)val << 8, MAKE_64BIT_MASK(8, 16));
        return;
    } else if ((pi & 7) == 3) {
        if (HAVE_al8) {
            store_atom_insert_al8(pv - 3, (uint64_t)val << 24, MAKE_64BIT_MASK(24, 16));
            return;
        }
    } else if ((pi & 15) == 7) {
        if (HAVE_CMPXCHG128) {
            Int128 v = int128_lshift(int128_make64(val), 56);
            Int128 m = int128_lshift(int128_make64(0xffff), 56);
            store_atom_insert_al16(pv - 7, v, m);
            return;
        }
    } else {
        g_assert_not_reached();
    }

    trace_store_atom2_fallback(memop, ra);
    cpu_loop_exit_atomic(cpu, ra);
}









static void store_atom_4(CPUState *cpu, uintptr_t ra,
                         void *pv, MemOp memop, uint32_t val)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;

    if (likely((pi & 3) == 0)) {
        store_atomic4(pv, val);
        return;
    }

    atmax = required_atomicity(cpu, pi, memop);
    switch (atmax) {
    case MO_8:
        stl_he_p(pv, val);
        return;
    case MO_16:
        store_atom_4_by_2(pv, val);
        return;
    case -MO_16:
        {
            uint32_t val_le = cpu_to_le32(val);
            int s2 = pi & 3;
            int s1 = 4 - s2;

            switch (s2) {
            case 1:
                val_le = store_whole_le4(pv, s1, val_le);
                *(uint8_t *)(pv + 3) = val_le;
                break;
            case 3:
                *(uint8_t *)pv = val_le;
                store_whole_le4(pv + 1, s2, val_le >> 8);
                break;
            case 0:
            case 2:
            default:
                g_assert_not_reached();
            }
        }
        return;
    case MO_32:
        if ((pi & 7) < 4) {
            if (HAVE_al8) {
                store_whole_le8(pv, 4, cpu_to_le32(val));
                return;
            }
        } else {
            if (HAVE_CMPXCHG128) {
                store_whole_le16(pv, 4, int128_make64(cpu_to_le32(val)));
                return;
            }
        }
        trace_store_atom4_fallback(memop, ra);
        cpu_loop_exit_atomic(cpu, ra);
    default:
        g_assert_not_reached();
    }
}









static void store_atom_8(CPUState *cpu, uintptr_t ra,
                         void *pv, MemOp memop, uint64_t val)
{
    uintptr_t pi = (uintptr_t)pv;
    int atmax;

    if (HAVE_al8 && likely((pi & 7) == 0)) {
        store_atomic8(pv, val);
        return;
    }

    atmax = required_atomicity(cpu, pi, memop);
    switch (atmax) {
    case MO_8:
        stq_he_p(pv, val);
        return;
    case MO_16:
        store_atom_8_by_2(pv, val);
        return;
    case MO_32:
        store_atom_8_by_4(pv, val);
        return;
    case -MO_32:
        if (HAVE_al8) {
            uint64_t val_le = cpu_to_le64(val);
            int s2 = pi & 7;
            int s1 = 8 - s2;

            switch (s2) {
            case 1 ... 3:
                val_le = store_whole_le8(pv, s1, val_le);
                store_bytes_leN(pv + s1, s2, val_le);
                break;
            case 5 ... 7:
                val_le = store_bytes_leN(pv, s1, val_le);
                store_whole_le8(pv + s1, s2, val_le);
                break;
            case 0:
            case 4:
            default:
                g_assert_not_reached();
            }
            return;
        }
        break;
    case MO_64:
        if (HAVE_CMPXCHG128) {
            store_whole_le16(pv, 8, int128_make64(cpu_to_le64(val)));
            return;
        }
        break;
    default:
        g_assert_not_reached();
    }
    trace_store_atom8_fallback(memop, ra);
    cpu_loop_exit_atomic(cpu, ra);
}









static void store_atom_16(CPUState *cpu, uintptr_t ra,
                          void *pv, MemOp memop, Int128 val)
{
    uintptr_t pi = (uintptr_t)pv;
    uint64_t a, b;
    int atmax;

    if (HAVE_ATOMIC128_RW && likely((pi & 15) == 0)) {
        atomic16_set(pv, val);
        return;
    }

    atmax = required_atomicity(cpu, pi, memop);

    a = HOST_BIG_ENDIAN ? int128_gethi(val) : int128_getlo(val);
    b = HOST_BIG_ENDIAN ? int128_getlo(val) : int128_gethi(val);
    switch (atmax) {
    case MO_8:
        memcpy(pv, &val, 16);
        return;
    case MO_16:
        store_atom_8_by_2(pv, a);
        store_atom_8_by_2(pv + 8, b);
        return;
    case MO_32:
        store_atom_8_by_4(pv, a);
        store_atom_8_by_4(pv + 8, b);
        return;
    case MO_64:
        if (HAVE_al8) {
            store_atomic8(pv, a);
            store_atomic8(pv + 8, b);
            return;
        }
        break;
    case -MO_64:
        if (HAVE_CMPXCHG128) {
            uint64_t val_le;
            int s2 = pi & 15;
            int s1 = 16 - s2;

            if (HOST_BIG_ENDIAN) {
                val = bswap128(val);
            }
            switch (s2) {
            case 1 ... 7:
                val_le = store_whole_le16(pv, s1, val);
                store_bytes_leN(pv + s1, s2, val_le);
                break;
            case 9 ... 15:
                store_bytes_leN(pv, s1, int128_getlo(val));
                val = int128_urshift(val, s1 * 8);
                store_whole_le16(pv + s1, s2, val);
                break;
            case 0:
            case 8:
            default:
                g_assert_not_reached();
            }
            return;
        }
        break;
    case MO_128:
        break;
    default:
        g_assert_not_reached();
    }
    trace_store_atom16_fallback(memop, ra);
    cpu_loop_exit_atomic(cpu, ra);
}
