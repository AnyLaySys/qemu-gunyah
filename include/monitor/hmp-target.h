
#ifndef MONITOR_HMP_TARGET_H
#define MONITOR_HMP_TARGET_H

typedef struct MonitorDef MonitorDef;

#ifdef COMPILING_PER_TARGET
#include "cpu.h"
struct MonitorDef {
    const char *name;
    int offset;
    target_long (*get_value)(Monitor *mon, const struct MonitorDef *md,
                             int val);
    int type;
};
#endif

#define MD_TLONG 0
#define MD_I32   1

const MonitorDef *target_monitor_defs(void);
int target_get_monitor_def(CPUState *cs, const char *name, uint64_t *pval);

CPUArchState *mon_get_cpu_env(Monitor *mon);
CPUState *mon_get_cpu(Monitor *mon);

void hmp_info_mem(Monitor *mon, const QDict *qdict);
void hmp_info_tlb(Monitor *mon, const QDict *qdict);
void hmp_mce(Monitor *mon, const QDict *qdict);
void hmp_info_local_apic(Monitor *mon, const QDict *qdict);
void hmp_info_sev(Monitor *mon, const QDict *qdict);
void hmp_info_sgx(Monitor *mon, const QDict *qdict);
void hmp_info_via(Monitor *mon, const QDict *qdict);
void hmp_memory_dump(Monitor *mon, const QDict *qdict);
void hmp_physical_memory_dump(Monitor *mon, const QDict *qdict);
void hmp_info_registers(Monitor *mon, const QDict *qdict);
void hmp_gva2gpa(Monitor *mon, const QDict *qdict);
void hmp_gpa2hva(Monitor *mon, const QDict *qdict);
void hmp_gpa2hpa(Monitor *mon, const QDict *qdict);

#endif /* MONITOR_HMP_TARGET_H */
