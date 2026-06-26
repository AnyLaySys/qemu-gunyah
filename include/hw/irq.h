#ifndef QEMU_IRQ_H
#define QEMU_IRQ_H

#include "qom/object.h"


#define TYPE_IRQ "irq"
OBJECT_DECLARE_SIMPLE_TYPE(IRQState, IRQ)

struct IRQState {
    Object parent_obj;

    qemu_irq_handler handler;
    void *opaque;
    int n;
};

void qemu_set_irq(qemu_irq irq, int level);

static inline void qemu_irq_raise(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
}

static inline void qemu_irq_lower(qemu_irq irq)
{
    qemu_set_irq(irq, 0);
}

static inline void qemu_irq_pulse(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
    qemu_set_irq(irq, 0);
}

void qemu_init_irq(IRQState *irq, qemu_irq_handler handler, void *opaque,
                   int n);

void qemu_init_irqs(IRQState irq[], size_t count,
                    qemu_irq_handler handler, void *opaque);

qemu_irq *qemu_allocate_irqs(qemu_irq_handler handler, void *opaque, int n);

qemu_irq qemu_allocate_irq(qemu_irq_handler handler, void *opaque, int n);

qemu_irq *qemu_extend_irqs(qemu_irq *old, int n_old, qemu_irq_handler handler,
                                void *opaque, int n);

void qemu_free_irqs(qemu_irq *s, int n);
void qemu_free_irq(qemu_irq irq);

qemu_irq qemu_irq_invert(qemu_irq irq);

void qemu_irq_intercept_in(qemu_irq *gpio_in, qemu_irq_handler handler, int n);

static inline bool qemu_irq_is_connected(qemu_irq irq)
{
    return irq != NULL;
}

#endif
