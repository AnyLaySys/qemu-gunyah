
#ifndef MIGRATION_REGISTER_H
#define MIGRATION_REGISTER_H

#include "hw/vmstate-if.h"

typedef struct StateHandlers {


    void (*save_state)(QEMUFile *f, void *opaque);

    int (*save_prepare)(void *opaque, Error **errp);

    int (*save_setup)(QEMUFile *f, void *opaque, Error **errp);

    void (*save_cleanup)(void *opaque);

    int (*save_live_complete_precopy)(QEMUFile *f, void *opaque);

    SaveLiveCompletePrecopyThreadHandler save_live_complete_precopy_thread;


    bool (*is_active)(void *opaque);

    bool (*is_active_iterate)(void *opaque);


    int (*save_live_iterate)(QEMUFile *f, void *opaque);


    void (*state_pending_estimate)(void *opaque, uint64_t *must_precopy,
                                   uint64_t *may_continue);

    void (*state_pending_exact)(void *opaque, uint64_t *must_precopy,
                                uint64_t *may_continue);

    int (*load_state)(QEMUFile *f, void *opaque, int version_id);

    bool (*load_state_buffer)(void *opaque, char *buf, size_t len,
                              Error **errp);

    int (*load_setup)(QEMUFile *f, void *opaque, Error **errp);

    int (*load_cleanup)(void *opaque);

    int (*resume_prepare)(MigrationState *s, void *opaque);

    bool (*switchover_ack_needed)(void *opaque);

    int (*switchover_start)(void *opaque);
} StateHandlers;

int register_state_live(const char *idstr,
                         uint32_t instance_id,
                         int version_id,
                         const StateHandlers *ops,
                         void *opaque);

void unregister_state(VMStateIf *obj, const char *idstr, void *opaque);

#endif
