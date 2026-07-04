
#ifndef QEMU_CO_SHARED_RESOURCE_H
#define QEMU_CO_SHARED_RESOURCE_H

typedef struct SharedResource SharedResource;

SharedResource *shres_create(uint64_t total);

void shres_destroy(SharedResource *s);

void coroutine_fn co_get_from_shres(SharedResource *s, uint64_t n);

void coroutine_fn co_put_to_shres(SharedResource *s, uint64_t n);


#endif /* QEMU_CO_SHARED_RESOURCE_H */
