








#ifndef TCG_ACCEL_OPS_RR_H
#define TCG_ACCEL_OPS_RR_H

#define TCG_KICK_PERIOD (NANOSECONDS_PER_SECOND / 10)


void rr_kick_vcpu_thread(CPUState *unused);


void rr_start_vcpu_thread(CPUState *cpu);

#endif
