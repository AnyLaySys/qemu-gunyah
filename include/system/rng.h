
#ifndef QEMU_RNG_H
#define QEMU_RNG_H

#include "qemu/queue.h"
#include "qom/object.h"

#define TYPE_RNG_BACKEND "rng-backend"
OBJECT_DECLARE_TYPE(RngBackend, RngBackendClass,
                    RNG_BACKEND)

#define TYPE_RNG_BUILTIN "rng-builtin"

typedef struct RngRequest RngRequest;

typedef void (EntropyReceiveFunc)(void *opaque,
                                  const void *data,
                                  size_t size);

struct RngRequest
{
    EntropyReceiveFunc *receive_entropy;
    uint8_t *data;
    void *opaque;
    size_t offset;
    size_t size;
    QSIMPLEQ_ENTRY(RngRequest) next;
};

struct RngBackendClass
{
    ObjectClass parent_class;

    void (*request_entropy)(RngBackend *s, RngRequest *req);

    void (*opened)(RngBackend *s, Error **errp);
};

struct RngBackend
{
    Object parent;

    bool opened;
    QSIMPLEQ_HEAD(, RngRequest) requests;
};


void rng_backend_request_entropy(RngBackend *s, size_t size,
                                 EntropyReceiveFunc *receive_entropy,
                                 void *opaque);

void rng_backend_finalize_request(RngBackend *s, RngRequest *req);
#endif
