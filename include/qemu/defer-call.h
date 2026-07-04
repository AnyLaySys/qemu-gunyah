
#ifndef QEMU_DEFER_CALL_H
#define QEMU_DEFER_CALL_H

void defer_call_begin(void);
void defer_call_end(void);
void defer_call(void (*fn)(void *), void *opaque);

#endif /* QEMU_DEFER_CALL_H */
