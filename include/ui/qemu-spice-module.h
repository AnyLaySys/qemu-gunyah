
#ifndef QEMU_SPICE_MODULE_H
#define QEMU_SPICE_MODULE_H

#ifdef CONFIG_SPICE
#include <spice.h>
#endif

typedef struct SpiceInfo SpiceInfo;

struct QemuSpiceOps {
    void (*init)(void);
    void (*display_init)(void);
    int (*migrate_info)(const char *h, int p, int t, const char *s);
    int (*set_passwd)(const char *passwd,
                      bool fail_if_connected, bool disconnect_if_connected);
    int (*set_pw_expire)(time_t expires);
    int (*display_add_client)(int csock, int skipauth, int tls);
#ifdef CONFIG_SPICE
    int (*add_interface)(SpiceBaseInstance *sin);
    SpiceInfo* (*qmp_query)(Error **errp);
#endif
};

extern int using_spice;
extern struct QemuSpiceOps qemu_spice;

#endif
