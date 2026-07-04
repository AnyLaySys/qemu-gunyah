
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/resettable.h"
#include "trace.h"

static void resettable_phase_enter(Object *obj, void *opaque, ResetType type);
static void resettable_phase_hold(Object *obj, void *opaque, ResetType type);
static void resettable_phase_exit(Object *obj, void *opaque, ResetType type);

static bool enter_phase_in_progress;
static unsigned exit_phase_in_progress;

void resettable_reset(Object *obj, ResetType type)
{
    trace_resettable_reset(obj, type);
    resettable_assert_reset(obj, type);
    resettable_release_reset(obj, type);
}

void resettable_assert_reset(Object *obj, ResetType type)
{
    trace_resettable_reset_assert_begin(obj, type);
    assert(!enter_phase_in_progress);

    enter_phase_in_progress = true;
    resettable_phase_enter(obj, NULL, type);
    enter_phase_in_progress = false;

    resettable_phase_hold(obj, NULL, type);

    trace_resettable_reset_assert_end(obj);
}

void resettable_release_reset(Object *obj, ResetType type)
{
    trace_resettable_reset_release_begin(obj, type);
    assert(!enter_phase_in_progress);

    exit_phase_in_progress += 1;
    resettable_phase_exit(obj, NULL, type);
    exit_phase_in_progress -= 1;

    trace_resettable_reset_release_end(obj);
}

bool resettable_is_in_reset(Object *obj)
{
    ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
    ResettableState *s = rc->get_state(obj);

    return s->count > 0;
}

static void resettable_child_foreach(ResettableClass *rc, Object *obj,
                                     ResettableChildCallback cb,
                                     void *opaque, ResetType type)
{
    if (rc->child_foreach) {
        rc->child_foreach(obj, cb, opaque, type);
    }
}

static void resettable_phase_enter(Object *obj, void *opaque, ResetType type)
{
    ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
    ResettableState *s = rc->get_state(obj);
    const char *obj_typename = object_get_typename(obj);
    bool action_needed = false;

    assert(!s->exit_phase_in_progress);

    trace_resettable_phase_enter_begin(obj, obj_typename, s->count, type);

    if (s->count++ == 0) {
        action_needed = true;
    }
    assert(s->count <= 50);

    resettable_child_foreach(rc, obj, resettable_phase_enter, NULL, type);

    if (action_needed) {
        trace_resettable_phase_enter_exec(obj, obj_typename, type,
                                          !!rc->phases.enter);
        if (rc->phases.enter) {
            rc->phases.enter(obj, type);
        }
        s->hold_phase_pending = true;
    }
    trace_resettable_phase_enter_end(obj, obj_typename, s->count);
}

static void resettable_phase_hold(Object *obj, void *opaque, ResetType type)
{
    ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
    ResettableState *s = rc->get_state(obj);
    const char *obj_typename = object_get_typename(obj);

    assert(!s->exit_phase_in_progress);

    trace_resettable_phase_hold_begin(obj, obj_typename, s->count, type);

    resettable_child_foreach(rc, obj, resettable_phase_hold, NULL, type);

    if (s->hold_phase_pending) {
        s->hold_phase_pending = false;
        trace_resettable_phase_hold_exec(obj, obj_typename, !!rc->phases.hold);
        if (rc->phases.hold) {
            rc->phases.hold(obj, type);
        }
    }
    trace_resettable_phase_hold_end(obj, obj_typename, s->count);
}

static void resettable_phase_exit(Object *obj, void *opaque, ResetType type)
{
    ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
    ResettableState *s = rc->get_state(obj);
    const char *obj_typename = object_get_typename(obj);

    assert(!s->exit_phase_in_progress);
    trace_resettable_phase_exit_begin(obj, obj_typename, s->count, type);

    s->exit_phase_in_progress = true;
    resettable_child_foreach(rc, obj, resettable_phase_exit, NULL, type);

    assert(s->count > 0);
    if (--s->count == 0) {
        trace_resettable_phase_exit_exec(obj, obj_typename, !!rc->phases.exit);
        if (rc->phases.exit) {
            rc->phases.exit(obj, type);
        }
    }
    s->exit_phase_in_progress = false;
    trace_resettable_phase_exit_end(obj, obj_typename, s->count);
}

static unsigned resettable_get_count(Object *obj)
{
    if (obj) {
        ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
        return rc->get_state(obj)->count;
    }
    return 0;
}

void resettable_change_parent(Object *obj, Object *newp, Object *oldp)
{
    ResettableClass *rc = RESETTABLE_GET_CLASS(obj);
    ResettableState *s = rc->get_state(obj);
    unsigned newp_count = resettable_get_count(newp);
    unsigned oldp_count = resettable_get_count(oldp);

    assert(!enter_phase_in_progress && !exit_phase_in_progress);
    trace_resettable_change_parent(obj, oldp, oldp_count, newp, newp_count);

    for (unsigned i = oldp_count; i < newp_count; i++) {
        resettable_assert_reset(obj, RESET_TYPE_COLD);
    }
    if (oldp_count && s->hold_phase_pending) {
        resettable_phase_hold(obj, NULL, RESET_TYPE_COLD);
    }
    for (unsigned i = newp_count; i < oldp_count; i++) {
        resettable_release_reset(obj, RESET_TYPE_COLD);
    }
}

void resettable_cold_reset_fn(void *opaque)
{
    resettable_reset((Object *) opaque, RESET_TYPE_COLD);
}

void resettable_class_set_parent_phases(ResettableClass *rc,
                                        ResettableEnterPhase enter,
                                        ResettableHoldPhase hold,
                                        ResettableExitPhase exit,
                                        ResettablePhases *parent_phases)
{
    *parent_phases = rc->phases;
    if (enter) {
        rc->phases.enter = enter;
    }
    if (hold) {
        rc->phases.hold = hold;
    }
    if (exit) {
        rc->phases.exit = exit;
    }
}

static const TypeInfo resettable_interface_info = {
    .name       = TYPE_RESETTABLE_INTERFACE,
    .parent     = TYPE_INTERFACE,
    .class_size = sizeof(ResettableClass),
};

static void reset_register_types(void)
{
    type_register_static(&resettable_interface_info);
}

type_init(reset_register_types)
