
#ifndef FIFO32_H
#define FIFO32_H

#include "qemu/fifo8.h"

typedef struct {
    Fifo8 fifo;
} Fifo32;


static inline void fifo32_create(Fifo32 *fifo, uint32_t capacity)
{
    fifo8_create(&fifo->fifo, capacity * sizeof(uint32_t));
}


static inline void fifo32_destroy(Fifo32 *fifo)
{
    fifo8_destroy(&fifo->fifo);
}


static inline uint32_t fifo32_num_free(Fifo32 *fifo)
{
    return DIV_ROUND_UP(fifo8_num_free(&fifo->fifo), sizeof(uint32_t));
}


static inline uint32_t fifo32_num_used(Fifo32 *fifo)
{
    return DIV_ROUND_UP(fifo8_num_used(&fifo->fifo), sizeof(uint32_t));
}


static inline void fifo32_push(Fifo32 *fifo, uint32_t data)
{
    int i;

    for (i = 0; i < sizeof(data); i++) {
        fifo8_push(&fifo->fifo, data & 0xff);
        data >>= 8;
    }
}


static inline void fifo32_push_all(Fifo32 *fifo, const uint32_t *data,
                                   uint32_t num)
{
    int i;

    for (i = 0; i < num; i++) {
        fifo32_push(fifo, data[i]);
    }
}


static inline uint32_t fifo32_pop(Fifo32 *fifo)
{
    uint32_t ret = 0;
    int i;

    for (i = 0; i < sizeof(uint32_t); i++) {
        ret |= (fifo8_pop(&fifo->fifo) << (i * 8));
    }

    return ret;
}



static inline void fifo32_reset(Fifo32 *fifo)
{
    fifo8_reset(&fifo->fifo);
}


static inline bool fifo32_is_empty(Fifo32 *fifo)
{
    return fifo8_is_empty(&fifo->fifo);
}


static inline bool fifo32_is_full(Fifo32 *fifo)
{
    return fifo8_num_free(&fifo->fifo) < sizeof(uint32_t);
}

#define VMSTATE_FIFO32(_field, _state) VMSTATE_FIFO8(_field.fifo, _state)

#endif /* FIFO32_H */
