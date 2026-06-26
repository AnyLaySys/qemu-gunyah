
#ifndef QEMU_MEMALIGN_H
#define QEMU_MEMALIGN_H

void *qemu_try_memalign(size_t alignment, size_t size);
void *qemu_memalign(size_t alignment, size_t size);
void qemu_vfree(void *ptr);
static inline void qemu_cleanup_generic_vfree(void *p)
{
  void **pp = (void **)p;
  qemu_vfree(*pp);
}

#define QEMU_AUTO_VFREE __attribute__((cleanup(qemu_cleanup_generic_vfree)))

#endif
