
#include "qemu/osdep.h"
#include "exec/cpu-common.h"
#include "hw/loader.h"
#include "net/net.h"
#include "system/cpus.h"
#include "system/system.h"

bool should_mlock(MlockState state)
{
    return state == MLOCK_ON || state == MLOCK_ON_FAULT;
}

bool is_mlock_on_fault(MlockState state)
{
    return state == MLOCK_ON_FAULT;
}

int display_opengl;
MlockState mlock_state;
bool enable_cpu_pm;
int autostart = 1;
Chardev *parallel_hds[MAX_PARALLEL_PORTS];
const char *qemu_name;
unsigned int nb_prom_envs;
const char *prom_envs[MAX_PROM_ENVS];
uint8_t *boot_splash_filedata;
int only_migratable; /* turn it off unless user states otherwise */

QemuUUID qemu_uuid;
bool qemu_uuid_set;
