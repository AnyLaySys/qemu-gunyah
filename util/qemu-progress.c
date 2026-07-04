
#include "qemu/osdep.h"
#include "qemu/qemu-progress.h"

struct progress_state {
    float current;
    float last_print;
    float min_skip;
    void (*print)(void);
    void (*end)(void);
};

static struct progress_state state;
static volatile sig_atomic_t print_pending;

static void progress_simple_print(void)
{
    printf("    (%3.2f/100%%)\r", state.current);
    fflush(stdout);
}

static void progress_simple_end(void)
{
    printf("\n");
}

static void progress_simple_init(void)
{
    state.print = progress_simple_print;
    state.end = progress_simple_end;
}

#ifdef CONFIG_POSIX
static void sigusr_print(int signal)
{
    print_pending = 1;
}
#endif

static void progress_dummy_print(void)
{
    if (print_pending) {
        fprintf(stderr, "    (%3.2f/100%%)\n", state.current);
        print_pending = 0;
    }
}

static void progress_dummy_end(void)
{
}

static void progress_dummy_init(void)
{
#ifdef CONFIG_POSIX
    struct sigaction action;
    sigset_t set;

    memset(&action, 0, sizeof(action));
    sigfillset(&action.sa_mask);
    action.sa_handler = sigusr_print;
    action.sa_flags = 0;
    sigaction(SIGUSR1, &action, NULL);
#ifdef SIGINFO
    sigaction(SIGINFO, &action, NULL);
#endif

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
#endif

    state.print = progress_dummy_print;
    state.end = progress_dummy_end;
}

void qemu_progress_init(int enabled, float min_skip)
{
    state.min_skip = min_skip;
    if (enabled) {
        progress_simple_init();
    } else {
        progress_dummy_init();
    }
}

void qemu_progress_end(void)
{
    state.end();
}

void qemu_progress_print(float delta, int max)
{
    float current;

    if (max == 0) {
        current = delta;
    } else {
        current = state.current + delta / 100 * max;
    }
    if (current > 100) {
        current = 100;
    }
    state.current = current;

    if (current > (state.last_print + state.min_skip) ||
        current < (state.last_print - state.min_skip) ||
        current == 100 || current == 0) {
        state.last_print = state.current;
        state.print();
    }
}
