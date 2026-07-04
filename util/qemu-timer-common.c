#include "qemu/osdep.h"
#include "qemu/timer.h"


int64_t clock_start;

#ifdef _WIN32

int64_t clock_freq;

static void __attribute__((constructor)) init_get_clock(void)
{
    LARGE_INTEGER freq;
    int ret;
    ret = QueryPerformanceFrequency(&freq);
    if (ret == 0) {
        fprintf(stderr, "Could not calibrate ticks\n");
        exit(1);
    }
    clock_freq = freq.QuadPart;
    clock_start = get_clock();
}

#else

int use_rt_clock;

static void __attribute__((constructor)) init_get_clock(void)
{
    struct timespec ts;

    use_rt_clock = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        use_rt_clock = 1;
    }
    clock_start = get_clock();
}
#endif
