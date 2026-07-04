
#ifndef MIGRATION_REGISTER_H
#define MIGRATION_REGISTER_H

#include "hw/vmstate-if.h"

typedef struct SaveVMHandlers {


    void (*save_state)(QEMUFile *f, void *opaque);

    int (*save_prepare)(void *opaque, Error **errp);

    int (*save_setup)(QEMUFile *f, void *opaque, Error **errp);

    void (*save_cleanup)(void *opaque);

    int (*save_live_complete_postcopy)(QEMUFile *f, void *opaque);

    int (*save_live_complete_precopy)(QEMUFile *f, void *opaque);

    SaveLiveCompletePrecopyThreadHandler save_live_complete_precopy_thread;


    bool (*is_active)(void *opaque);

    bool (*has_postcopy)(void *opaque);

    bool (*is_active_iterate)(void *opaque);


    int (*save_live_iterate)(QEMUFile *f, void *opaque);


    void (*state_pending_estimate)(void *opaque, uint64_t *must_precopy,
                                   uint64_t *can_postcopy);

    void (*state_pending_exact)(void *opaque, uint64_t *must_precopy,
                                uint64_t *can_postcopy);

    int (*load_state)(QEMUFile *f, void *opaque, int version_id);

    bool (*load_state_buffer)(void *opaque, char *buf, size_t len,
                              Error **errp);

    int (*load_setup)(QEMUFile *f, void *opaque, Error **errp);

    int (*load_cleanup)(void *opaque);

    int (*resume_prepare)(MigrationState *s, void *opaque);

    bool (*switchover_ack_needed)(void *opaque);

    int (*switchover_start)(void *opaque);
} SaveVMHandlers;

int register_savevm_live(const char *idstr,
                         uint32_t instance_id,
                         int version_id,
                         const SaveVMHandlers *ops,
                         void *opaque);

void unregister_savevm(VMStateIf *obj, const char *idstr, void *opaque);

#endif
