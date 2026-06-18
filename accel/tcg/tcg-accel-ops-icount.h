








#ifndef TCG_ACCEL_OPS_ICOUNT_H
#define TCG_ACCEL_OPS_ICOUNT_H

void icount_handle_deadline(void);
void icount_prepare_for_run(CPUState *cpu, int64_t cpu_budget);
int64_t icount_percpu_budget(int cpu_count);
void icount_process_data(CPUState *cpu);

void icount_handle_interrupt(CPUState *cpu, int mask);

#endif
