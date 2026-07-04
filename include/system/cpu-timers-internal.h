
#ifndef TIMERS_STATE_H
#define TIMERS_STATE_H


typedef struct TimersState {
    int64_t cpu_ticks_prev;
    int64_t cpu_ticks_offset;

    QemuSeqLock vm_clock_seqlock;
    QemuSpin vm_clock_lock;

    int16_t cpu_ticks_enabled;

    int16_t icount_time_shift;
    int64_t last_delta;

    aligned_int64_t qemu_icount_bias;

    int64_t vm_clock_warp_start;
    int64_t cpu_clock_offset;

    int64_t qemu_icount;

    QEMUTimer *icount_rt_timer;
    QEMUTimer *icount_vm_timer;
    QEMUTimer *icount_warp_timer;
} TimersState;

extern TimersState timers_state;

int64_t cpu_get_clock_locked(void);

#endif /* TIMERS_STATE_H */
