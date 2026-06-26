
#ifndef SYSTEM_HOSTMEM_H
#define SYSTEM_HOSTMEM_H

#include "system/numa.h"
#include "qapi/qapi-types-machine.h"
#include "qom/object.h"
#include "exec/memory.h"
#include "qemu/bitmap.h"
#include "qemu/thread-context.h"

#define TYPE_MEMORY_BACKEND "memory-backend"
OBJECT_DECLARE_TYPE(HostMemoryBackend, HostMemoryBackendClass,
                    MEMORY_BACKEND)


#define TYPE_MEMORY_BACKEND_RAM "memory-backend-ram"

#define TYPE_MEMORY_BACKEND_FILE "memory-backend-file"

#define TYPE_MEMORY_BACKEND_MEMFD "memory-backend-memfd"


struct HostMemoryBackendClass {
    ObjectClass parent_class;

    bool (*alloc)(HostMemoryBackend *backend, Error **errp);
};

struct HostMemoryBackend {
    Object parent;

    uint64_t size;
    bool merge, dump, use_canonical_path;
    bool prealloc, is_mapped, share, reserve;
    bool guest_memfd, aligned;
    uint32_t prealloc_threads;
    ThreadContext *prealloc_context;
    DECLARE_BITMAP(host_nodes, MAX_NODES + 1);
    HostMemPolicy policy;

    MemoryRegion mr;
};

bool host_memory_backend_mr_inited(HostMemoryBackend *backend);
MemoryRegion *host_memory_backend_get_memory(HostMemoryBackend *backend);

void host_memory_backend_set_mapped(HostMemoryBackend *backend, bool mapped);
bool host_memory_backend_is_mapped(HostMemoryBackend *backend);
size_t host_memory_backend_pagesize(HostMemoryBackend *memdev);
char *host_memory_backend_get_name(HostMemoryBackend *backend);

long qemu_minrampagesize(void);
long qemu_maxrampagesize(void);

#endif
