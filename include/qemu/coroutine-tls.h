
#ifndef QEMU_COROUTINE_TLS_H
#define QEMU_COROUTINE_TLS_H


#define QEMU_DECLARE_CO_TLS(type, var)                                       \
    __attribute__((noinline)) type get_##var(void);                          \
    __attribute__((noinline)) void set_##var(type v);                        \
    __attribute__((noinline)) type *get_ptr_##var(void);




#define QEMU_DEFINE_CO_TLS(type, var)                                        \
    static pthread_key_t co_tls_key_##var;                                   \
    static pthread_once_t co_tls_once_##var = PTHREAD_ONCE_INIT;             \
    static void co_tls_init_##var(void) {                                    \
        pthread_key_create(&co_tls_key_##var, free);                         \
    }                                                                        \
    static inline type *co_tls_get_ptr_##var(void) {                         \
        pthread_once(&co_tls_once_##var, co_tls_init_##var);                 \
        type *p = (type *)pthread_getspecific(co_tls_key_##var);             \
        if (!p) {                                                            \
            p = (type *)calloc(1, sizeof(type));                             \
            pthread_setspecific(co_tls_key_##var, p);                        \
        }                                                                    \
        return p;                                                            \
    }                                                                        \
    type get_##var(void) { return *co_tls_get_ptr_##var(); }                 \
    void set_##var(type v) { *co_tls_get_ptr_##var() = v; }                  \
    type *get_ptr_##var(void) { return co_tls_get_ptr_##var(); }

#define QEMU_DEFINE_STATIC_CO_TLS(type, var)                                 \
    static pthread_key_t co_tls_key_##var;                                   \
    static pthread_once_t co_tls_once_##var = PTHREAD_ONCE_INIT;             \
    static void co_tls_init_##var(void) {                                    \
        pthread_key_create(&co_tls_key_##var, free);                         \
    }                                                                        \
    static inline type *co_tls_get_ptr_##var(void) {                         \
        pthread_once(&co_tls_once_##var, co_tls_init_##var);                 \
        type *p = (type *)pthread_getspecific(co_tls_key_##var);             \
        if (!p) {                                                            \
            p = (type *)calloc(1, sizeof(type));                             \
            pthread_setspecific(co_tls_key_##var, p);                        \
        }                                                                    \
        return p;                                                            \
    }                                                                        \
    static __attribute__((unused))                                           \
    type get_##var(void) { return *co_tls_get_ptr_##var(); }                 \
    static __attribute__((unused))                                           \
    void set_##var(type v) { *co_tls_get_ptr_##var() = v; }                  \
    static __attribute__((unused))                                           \
    type *get_ptr_##var(void) { return co_tls_get_ptr_##var(); }


#endif /* QEMU_COROUTINE_TLS_H */
