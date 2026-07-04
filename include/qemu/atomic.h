
#ifndef QEMU_ATOMIC_H
#define QEMU_ATOMIC_H

#include "compiler.h"

#define barrier()   ({ asm volatile("" ::: "memory"); (void)0; })

#ifndef __ATOMIC_RELAXED
#error "Expecting C11 atomic ops"
#endif


#define smp_mb()                     ({ barrier(); __atomic_thread_fence(__ATOMIC_SEQ_CST); })
#define smp_mb_release()             ({ barrier(); __atomic_thread_fence(__ATOMIC_RELEASE); })
#define smp_mb_acquire()             ({ barrier(); __atomic_thread_fence(__ATOMIC_ACQUIRE); })

#ifdef QEMU_SANITIZE_THREAD
#define smp_read_barrier_depends()   ({ barrier(); __atomic_thread_fence(__ATOMIC_CONSUME); })
#elif defined(__alpha__)
#define smp_read_barrier_depends()   asm volatile("mb":::"memory")
#else
#define smp_read_barrier_depends()   barrier()
#endif

#define signal_barrier()    __atomic_signal_fence(__ATOMIC_SEQ_CST)

#define ATOMIC_REG_SIZE  sizeof(void *)

#define qatomic_read__nocheck(ptr) \
    __atomic_load_n(ptr, __ATOMIC_RELAXED)

#define qatomic_read(ptr)                              \
    ({                                                 \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    qatomic_read__nocheck(ptr);                        \
    })

#define qatomic_set__nocheck(ptr, i) \
    __atomic_store_n(ptr, i, __ATOMIC_RELAXED)

#define qatomic_set(ptr, i)  do {                      \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    qatomic_set__nocheck(ptr, i);                      \
} while(0)

#ifdef QEMU_SANITIZE_THREAD
#define qatomic_rcu_read__nocheck(ptr, valptr)           \
    __atomic_load(ptr, valptr, __ATOMIC_CONSUME);
#else
#define qatomic_rcu_read__nocheck(ptr, valptr)           \
    __atomic_load(ptr, valptr, __ATOMIC_RELAXED);        \
    smp_read_barrier_depends();
#endif

#define qatomic_rcu_read_internal(ptr, _val)            \
    ({                                                  \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    typeof_strip_qual(*ptr) _val;                       \
    qatomic_rcu_read__nocheck(ptr, &_val);              \
    _val;                                               \
    })
#define qatomic_rcu_read(ptr) \
    qatomic_rcu_read_internal((ptr), MAKE_IDENTIFIER(_val))

#define qatomic_rcu_set(ptr, i) do {                   \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    __atomic_store_n(ptr, i, __ATOMIC_RELEASE);        \
} while(0)

#define qatomic_load_acquire(ptr)                       \
    ({                                                  \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    typeof_strip_qual(*ptr) _val;                       \
    __atomic_load(ptr, &_val, __ATOMIC_ACQUIRE);        \
    _val;                                               \
    })

#define qatomic_store_release(ptr, i)  do {             \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE); \
    __atomic_store_n(ptr, i, __ATOMIC_RELEASE);         \
} while(0)



#define qatomic_xchg__nocheck(ptr, i)    ({                 \
    __atomic_exchange_n(ptr, (i), __ATOMIC_SEQ_CST);        \
})

#define qatomic_xchg(ptr, i)    ({                          \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE);     \
    qatomic_xchg__nocheck(ptr, i);                          \
})

#define qatomic_cmpxchg__nocheck(ptr, old, new)    ({                   \
    typeof_strip_qual(*ptr) _old = (old);                               \
    (void)__atomic_compare_exchange_n(ptr, &_old, new, false,           \
                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);      \
    _old;                                                               \
})

#define qatomic_cmpxchg(ptr, old, new)    ({                            \
    qemu_build_assert(sizeof(*ptr) <= ATOMIC_REG_SIZE);                 \
    qatomic_cmpxchg__nocheck(ptr, old, new);                            \
})

#define qatomic_fetch_inc(ptr)  __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST)
#define qatomic_fetch_dec(ptr)  __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST)

#define qatomic_fetch_add(ptr, n) __atomic_fetch_add(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_fetch_sub(ptr, n) __atomic_fetch_sub(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_fetch_and(ptr, n) __atomic_fetch_and(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_fetch_or(ptr, n)  __atomic_fetch_or(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_fetch_xor(ptr, n) __atomic_fetch_xor(ptr, n, __ATOMIC_SEQ_CST)

#define qatomic_inc_fetch(ptr)    __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define qatomic_dec_fetch(ptr)    __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define qatomic_add_fetch(ptr, n) __atomic_add_fetch(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_sub_fetch(ptr, n) __atomic_sub_fetch(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_and_fetch(ptr, n) __atomic_and_fetch(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_or_fetch(ptr, n)  __atomic_or_fetch(ptr, n, __ATOMIC_SEQ_CST)
#define qatomic_xor_fetch(ptr, n) __atomic_xor_fetch(ptr, n, __ATOMIC_SEQ_CST)

#define qatomic_inc(ptr) \
    ((void) __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST))
#define qatomic_dec(ptr) \
    ((void) __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST))
#define qatomic_add(ptr, n) \
    ((void) __atomic_fetch_add(ptr, n, __ATOMIC_SEQ_CST))
#define qatomic_sub(ptr, n) \
    ((void) __atomic_fetch_sub(ptr, n, __ATOMIC_SEQ_CST))
#define qatomic_and(ptr, n) \
    ((void) __atomic_fetch_and(ptr, n, __ATOMIC_SEQ_CST))
#define qatomic_or(ptr, n) \
    ((void) __atomic_fetch_or(ptr, n, __ATOMIC_SEQ_CST))
#define qatomic_xor(ptr, n) \
    ((void) __atomic_fetch_xor(ptr, n, __ATOMIC_SEQ_CST))

#define smp_wmb()   smp_mb_release()
#define smp_rmb()   smp_mb_acquire()

#if !defined(QEMU_SANITIZE_THREAD) && \
    (defined(__i386__) || defined(__x86_64__) || defined(__s390x__))
# define smp_mb__before_rmw() signal_barrier()
# define smp_mb__after_rmw() signal_barrier()
#else
# define smp_mb__before_rmw() smp_mb()
# define smp_mb__after_rmw() smp_mb()
#endif


#if !defined(QEMU_SANITIZE_THREAD) && \
    (defined(__i386__) || defined(__x86_64__) || defined(__s390x__))
# define qatomic_set_mb(ptr, i) \
    ({ (void)qatomic_xchg(ptr, i); smp_mb__after_rmw(); })
#else
# define qatomic_set_mb(ptr, i) \
   ({ qatomic_store_release(ptr, i); smp_mb(); })
#endif

#define qatomic_fetch_inc_nonzero(ptr) ({                               \
    typeof_strip_qual(*ptr) _oldn = qatomic_read(ptr);                  \
    while (_oldn && qatomic_cmpxchg(ptr, _oldn, _oldn + 1) != _oldn) {  \
        _oldn = qatomic_read(ptr);                                      \
    }                                                                   \
    _oldn;                                                              \
})

typedef int64_t aligned_int64_t __attribute__((aligned(8)));
typedef uint64_t aligned_uint64_t __attribute__((aligned(8)));

#ifdef CONFIG_ATOMIC64
#define qatomic_read_i64(P) \
    _Generic(*(P), int64_t: qatomic_read__nocheck(P))
#define qatomic_read_u64(P) \
    _Generic(*(P), uint64_t: qatomic_read__nocheck(P))
#define qatomic_set_i64(P, V) \
    _Generic(*(P), int64_t: qatomic_set__nocheck(P, V))
#define qatomic_set_u64(P, V) \
    _Generic(*(P), uint64_t: qatomic_set__nocheck(P, V))

static inline void qatomic64_init(void)
{
}
#else /* !CONFIG_ATOMIC64 */
int64_t  qatomic_read_i64(const int64_t *ptr);
uint64_t qatomic_read_u64(const uint64_t *ptr);
void qatomic_set_i64(int64_t *ptr, int64_t val);
void qatomic_set_u64(uint64_t *ptr, uint64_t val);
void qatomic64_init(void);
#endif /* !CONFIG_ATOMIC64 */

#endif /* QEMU_ATOMIC_H */
