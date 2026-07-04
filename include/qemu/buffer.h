
#ifndef QEMU_BUFFER_H
#define QEMU_BUFFER_H


typedef struct Buffer Buffer;


struct Buffer {
    char *name;
    size_t capacity;
    size_t offset;
    uint64_t avg_size;
    uint8_t *buffer;
};

void buffer_init(Buffer *buffer, const char *name, ...)
        G_GNUC_PRINTF(2, 3);

void buffer_shrink(Buffer *buffer);

void buffer_reserve(Buffer *buffer, size_t len);

void buffer_reset(Buffer *buffer);

void buffer_free(Buffer *buffer);

void buffer_append(Buffer *buffer, const void *data, size_t len);

void buffer_advance(Buffer *buffer, size_t len);

uint8_t *buffer_end(Buffer *buffer);

gboolean buffer_empty(Buffer *buffer);

void buffer_move_empty(Buffer *to, Buffer *from);

void buffer_move(Buffer *to, Buffer *from);

#endif /* QEMU_BUFFER_H */
