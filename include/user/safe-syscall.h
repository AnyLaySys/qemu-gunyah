
#ifndef LINUX_USER_SAFE_SYSCALL_H
#define LINUX_USER_SAFE_SYSCALL_H



long safe_syscall_base(int *pending, long number, ...);
long safe_syscall_set_errno_tail(int value);

extern char safe_syscall_start[];
extern char safe_syscall_end[];

#define safe_syscall(...)                                                 \
    safe_syscall_base(&get_task_state(thread_cpu)->signal_pending,        \
                      __VA_ARGS__)

#endif
