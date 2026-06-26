
#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qemu/guest-random.h"
#include "crypto/random.h"

#ifdef __ANDROID__
#include <pthread.h>
static pthread_key_t thread_rand_key;
static pthread_once_t thread_rand_once = PTHREAD_ONCE_INIT;

static void thread_rand_destroy(void *p) {
    if (p) g_rand_free((GRand *)p);
}

static void thread_rand_key_init(void) {
    pthread_key_create(&thread_rand_key, thread_rand_destroy);
}

static GRand *get_thread_rand(void) {
    pthread_once(&thread_rand_once, thread_rand_key_init);
    return (GRand *)pthread_getspecific(thread_rand_key);
}

static void set_thread_rand(GRand *rand) {
    pthread_once(&thread_rand_once, thread_rand_key_init);
    pthread_setspecific(thread_rand_key, rand);
}
#else
static __thread GRand *thread_rand;
static GRand *get_thread_rand(void) { return thread_rand; }
static void set_thread_rand(GRand *rand) { thread_rand = rand; }
#endif

static bool deterministic;


static int glib_random_bytes(void *buf, size_t len)
{
    GRand *rand = get_thread_rand();
    size_t i;
    uint32_t x;

    if (unlikely(rand == NULL)) {
        rand = g_rand_new();
        set_thread_rand(rand);
    }

    for (i = 0; i + 4 <= len; i += 4) {
        x = g_rand_int(rand);
        __builtin_memcpy(buf + i, &x, 4);
    }
    if (i < len) {
        x = g_rand_int(rand);
        __builtin_memcpy(buf + i, &x, len - i);
    }
    return 0;
}

int qemu_guest_getrandom(void *buf, size_t len, Error **errp)
{
    if (unlikely(deterministic)) {
        return glib_random_bytes(buf, len);
    }
    return qcrypto_random_bytes(buf, len, errp);
}

void qemu_guest_getrandom_nofail(void *buf, size_t len)
{
    (void)qemu_guest_getrandom(buf, len, &error_fatal);
}

uint64_t qemu_guest_random_seed_thread_part1(void)
{
    if (deterministic) {
        uint64_t ret;
        glib_random_bytes(&ret, sizeof(ret));
        return ret;
    }
    return 0;
}

void qemu_guest_random_seed_thread_part2(uint64_t seed)
{
    g_assert(get_thread_rand() == NULL);
    if (deterministic) {
        set_thread_rand(
            g_rand_new_with_seed_array((const guint32 *)&seed,
                                       sizeof(seed) / sizeof(guint32)));
    }
}

int qemu_guest_random_seed_main(const char *seedstr, Error **errp)
{
    uint64_t seed;
    if (parse_uint_full(seedstr, 0, &seed)) {
        error_setg(errp, "Invalid seed number: %s", seedstr);
        return -1;
    } else {
        deterministic = true;
        qemu_guest_random_seed_thread_part2(seed);
        return 0;
    }
}
