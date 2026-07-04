
#ifndef HW_RESETTABLE_H
#define HW_RESETTABLE_H

#include "qom/object.h"

#define TYPE_RESETTABLE_INTERFACE "resettable"

typedef struct ResettableClass ResettableClass;
DECLARE_CLASS_CHECKERS(ResettableClass, RESETTABLE,
                       TYPE_RESETTABLE_INTERFACE)


typedef struct ResettableState ResettableState;

typedef enum ResetType {
    RESET_TYPE_COLD,
    RESET_TYPE_SNAPSHOT_LOAD,
    RESET_TYPE_WAKEUP,
    RESET_TYPE_S390_CPU_INITIAL,
    RESET_TYPE_S390_CPU_NORMAL,
} ResetType;

typedef void (*ResettableEnterPhase)(Object *obj, ResetType type);
typedef void (*ResettableHoldPhase)(Object *obj, ResetType type);
typedef void (*ResettableExitPhase)(Object *obj, ResetType type);
typedef ResettableState * (*ResettableGetState)(Object *obj);
typedef void (*ResettableChildCallback)(Object *, void *opaque,
                                        ResetType type);
typedef void (*ResettableChildForeach)(Object *obj,
                                       ResettableChildCallback cb,
                                       void *opaque, ResetType type);
typedef struct ResettablePhases {
    ResettableEnterPhase enter;
    ResettableHoldPhase hold;
    ResettableExitPhase exit;
} ResettablePhases;
struct ResettableClass {
    InterfaceClass parent_class;

    ResettablePhases phases;

    ResettableGetState get_state;

    ResettableChildForeach child_foreach;
};

struct ResettableState {
    unsigned count;
    bool hold_phase_pending;
    bool exit_phase_in_progress;
};

static inline void resettable_state_clear(ResettableState *state)
{
    memset(state, 0, sizeof(ResettableState));
}

void resettable_reset(Object *obj, ResetType type);

void resettable_assert_reset(Object *obj, ResetType type);

void resettable_release_reset(Object *obj, ResetType type);

bool resettable_is_in_reset(Object *obj);

void resettable_change_parent(Object *obj, Object *newp, Object *oldp);

void resettable_cold_reset_fn(void *opaque);

void resettable_class_set_parent_phases(ResettableClass *rc,
                                        ResettableEnterPhase enter,
                                        ResettableHoldPhase hold,
                                        ResettableExitPhase exit,
                                        ResettablePhases *parent_phases);

#endif
