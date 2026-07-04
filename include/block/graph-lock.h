#ifndef GRAPH_LOCK_H
#define GRAPH_LOCK_H

typedef struct BdrvGraphRWlock BdrvGraphRWlock;

typedef struct TSA_CAPABILITY("mutex") BdrvGraphLock {
} BdrvGraphLock;

extern BdrvGraphLock graph_lock;

#define GRAPH_WRLOCK TSA_REQUIRES(graph_lock)
#define GRAPH_RDLOCK TSA_REQUIRES_SHARED(graph_lock)
#define GRAPH_UNLOCKED TSA_EXCLUDES(graph_lock)

#define GRAPH_RDLOCK_PTR TSA_GUARDED_BY(graph_lock)
#define GRAPH_WRLOCK_PTR TSA_GUARDED_BY(graph_lock)
#define GRAPH_UNLOCKED_PTR

void register_aiocontext(AioContext *ctx);

void unregister_aiocontext(AioContext *ctx);

void no_coroutine_fn TSA_ACQUIRE(graph_lock) TSA_NO_TSA
bdrv_graph_wrlock(void);

void no_coroutine_fn TSA_RELEASE(graph_lock) TSA_NO_TSA
bdrv_graph_wrunlock(void);

void coroutine_fn TSA_ACQUIRE_SHARED(graph_lock) TSA_NO_TSA
bdrv_graph_co_rdlock(void);

void coroutine_fn TSA_RELEASE_SHARED(graph_lock) TSA_NO_TSA
bdrv_graph_co_rdunlock(void);

void TSA_ACQUIRE_SHARED(graph_lock) TSA_NO_TSA
bdrv_graph_rdlock_main_loop(void);

void TSA_RELEASE_SHARED(graph_lock) TSA_NO_TSA
bdrv_graph_rdunlock_main_loop(void);

void GRAPH_RDLOCK assert_bdrv_graph_readable(void);

void GRAPH_WRLOCK assert_bdrv_graph_writable(void);

static inline void TSA_ASSERT_SHARED(graph_lock) TSA_NO_TSA
assume_graph_lock(void)
{
}

typedef struct GraphLockable { } GraphLockable;

#define GML_OBJ_() (&(GraphLockable) { })

static inline GraphLockable * TSA_ACQUIRE_SHARED(graph_lock) coroutine_fn
graph_lockable_auto_lock(GraphLockable *x)
{
    bdrv_graph_co_rdlock();
    return x;
}

static inline void TSA_RELEASE_SHARED(graph_lock) coroutine_fn
graph_lockable_auto_unlock(GraphLockable **x)
{
    bdrv_graph_co_rdunlock();
}

#define GRAPH_AUTO_UNLOCK __attribute__((cleanup(graph_lockable_auto_unlock)))

#define WITH_GRAPH_RDLOCK_GUARD_(var)                                         \
    for (GraphLockable *unlock_var GRAPH_AUTO_UNLOCK =                        \
            graph_lockable_auto_lock(GML_OBJ_()),                             \
            *var = unlock_var;                                                \
         var;                                                                 \
         var = NULL)

#define WITH_GRAPH_RDLOCK_GUARD() \
    WITH_GRAPH_RDLOCK_GUARD_(glue(graph_lockable_auto, __COUNTER__))

#define GRAPH_RDLOCK_GUARD(x)                                       \
    GraphLockable * GRAPH_AUTO_UNLOCK                               \
    glue(graph_lockable_auto, __COUNTER__) G_GNUC_UNUSED =          \
            graph_lockable_auto_lock(GML_OBJ_())


typedef struct GraphLockableMainloop { } GraphLockableMainloop;

#define GMLML_OBJ_() (&(GraphLockableMainloop) { })

static inline GraphLockableMainloop * TSA_ASSERT_SHARED(graph_lock) TSA_NO_TSA
graph_lockable_auto_lock_mainloop(GraphLockableMainloop *x)
{
    bdrv_graph_rdlock_main_loop();
    return x;
}

static inline void TSA_NO_TSA
graph_lockable_auto_unlock_mainloop(GraphLockableMainloop *x)
{
    bdrv_graph_rdunlock_main_loop();
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GraphLockableMainloop,
                              graph_lockable_auto_unlock_mainloop)

#define GRAPH_RDLOCK_GUARD_MAINLOOP(x)                              \
    g_autoptr(GraphLockableMainloop)                                \
    glue(graph_lockable_auto, __COUNTER__) G_GNUC_UNUSED =          \
            graph_lockable_auto_lock_mainloop(GMLML_OBJ_())

#endif /* GRAPH_LOCK_H */

