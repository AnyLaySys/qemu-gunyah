#ifndef SYSTEM_H
#define SYSTEM_H

#include "qemu/timer.h"
#include "qemu/notify.h"
#include "qemu/uuid.h"


extern int only_migratable;
extern const char *qemu_name;
extern QemuUUID qemu_uuid;
extern bool qemu_uuid_set;

const char *qemu_get_vm_name(void);

void qemu_add_exit_notifier(Notifier *notify);
void qemu_remove_exit_notifier(Notifier *notify);

void qemu_add_machine_init_done_notifier(Notifier *notify);
void qemu_remove_machine_init_done_notifier(Notifier *notify);

void configure_rtc(QemuOpts *opts);

void qemu_init_subsystems(void);

extern int autostart;

extern int graphic_width;
extern int graphic_height;
extern int graphic_depth;
extern int display_opengl;
extern uint8_t *boot_splash_filedata;
extern bool enable_cpu_pm;
extern QEMUClockType rtc_clock;

typedef enum {
    MLOCK_OFF = 0,
    MLOCK_ON,
    MLOCK_ON_FAULT,
} MlockState;

bool should_mlock(MlockState);
bool is_mlock_on_fault(MlockState);

extern MlockState mlock_state;

#define MAX_PROM_ENVS 128
extern const char *prom_envs[MAX_PROM_ENVS];
extern unsigned int nb_prom_envs;


Chardev *serial_hd(int i);


#define MAX_PARALLEL_PORTS 3

extern Chardev *parallel_hds[MAX_PARALLEL_PORTS];

void add_boot_device_path(int32_t bootindex, DeviceState *dev,
                          const char *suffix);
char *get_boot_devices_list(size_t *size);

DeviceState *get_boot_device(uint32_t position);
void check_boot_index(int32_t bootindex, Error **errp);
void del_boot_device_path(DeviceState *dev, const char *suffix);
void device_add_bootindex_property(Object *obj, int32_t *bootindex,
                                   const char *name, const char *suffix,
                                   DeviceState *dev);
void restore_boot_order(void *opaque);
void validate_bootdevices(const char *devices, Error **errp);
void add_boot_device_lchs(DeviceState *dev, const char *suffix,
                          uint32_t lcyls, uint32_t lheads, uint32_t lsecs);
void del_boot_device_lchs(DeviceState *dev, const char *suffix);
char *get_boot_devices_lchs_list(size_t *size);

typedef void QEMUBootSetHandler(void *opaque, const char *boot_order,
                                Error **errp);
void qemu_register_boot_set(QEMUBootSetHandler *func, void *opaque);
void qemu_boot_set(const char *boot_order, Error **errp);

bool defaults_enabled(void);

void qemu_init(int argc, char **argv);
int qemu_main_loop(void);
void qemu_cleanup(int);

extern QemuOptsList qemu_legacy_drive_opts;
extern QemuOptsList qemu_common_drive_opts;
extern QemuOptsList qemu_drive_opts;
extern QemuOptsList bdrv_runtime_opts;
extern QemuOptsList qemu_chardev_opts;
extern QemuOptsList qemu_device_opts;
extern QemuOptsList qemu_netdev_opts;
extern QemuOptsList qemu_nic_opts;
extern QemuOptsList qemu_net_opts;
extern QemuOptsList qemu_global_opts;

#endif
