
#include "qemu/osdep.h"
#include <dirent.h>
#include <sys/prctl.h>
#include <sched.h>

#include "qemu/async-teardown.h"

#ifdef _SC_THREAD_STACK_MIN
#define CLONE_STACK_SIZE sysconf(_SC_THREAD_STACK_MIN)
#else
#define CLONE_STACK_SIZE 16384
#endif

static pid_t the_ppid;

static void hup_handler(int signal)
{
    while (the_ppid == getppid()) {
        sleep(1);
    }

    _exit(0);
}

static int async_teardown_fn(void *arg)
{
    struct sigaction sa = { .sa_handler = hup_handler };
    sigset_t hup_signal;
    char name[16];

    snprintf(name, 16, "cleanup/%d", the_ppid);
    prctl(PR_SET_NAME, (unsigned long)name);

    qemu_close_all_open_fd(NULL, 0);

    sigaction(SIGHUP, &sa, NULL);
    sigemptyset(&hup_signal);
    sigaddset(&hup_signal, SIGHUP);
    sigprocmask(SIG_UNBLOCK, &hup_signal, NULL);

    prctl(PR_SET_PDEATHSIG, SIGHUP);

    if (the_ppid == getppid()) {
        pause();
    }

    _exit(0);
}

static void *new_stack_for_clone(void)
{
    size_t stack_size = CLONE_STACK_SIZE;
    char *stack_ptr;

    stack_ptr = qemu_alloc_stack(&stack_size);
    stack_ptr += stack_size;

    return stack_ptr;
}

void init_async_teardown(void)
{
    sigset_t all_signals, old_signals;

    the_ppid = getpid();

    sigfillset(&all_signals);
    sigprocmask(SIG_BLOCK, &all_signals, &old_signals);
    clone(async_teardown_fn, new_stack_for_clone(), CLONE_VM, NULL);
    sigprocmask(SIG_SETMASK, &old_signals, NULL);
}
