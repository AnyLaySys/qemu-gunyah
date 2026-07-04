
#ifndef CHARDEV_INTERNAL_H
#define CHARDEV_INTERNAL_H

#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define MAX_MUX 4
#define MUX_BUFFER_SIZE 32 /* Must be a power of 2.  */
#define MUX_BUFFER_MASK (MUX_BUFFER_SIZE - 1)

struct MuxChardev {
    Chardev parent;
    CharBackend *backends[MAX_MUX];
    CharBackend chr;
    unsigned long mux_bitset;
    int focus;
    bool term_got_escape;
    unsigned char buffer[MAX_MUX][MUX_BUFFER_SIZE];
    unsigned int prod[MAX_MUX];
    unsigned int cons[MAX_MUX];
    int timestamps;

    bool linestart;
    int64_t timestamps_start;
};
typedef struct MuxChardev MuxChardev;

DECLARE_INSTANCE_CHECKER(MuxChardev, MUX_CHARDEV,
                         TYPE_CHARDEV_MUX)

#define CHARDEV_IS_MUX(chr)                                \
    object_dynamic_cast(OBJECT(chr), TYPE_CHARDEV_MUX)

bool mux_chr_attach_frontend(MuxChardev *d, CharBackend *b,
                             unsigned int *tag, Error **errp);
bool mux_chr_detach_frontend(MuxChardev *d, unsigned int tag);
void mux_set_focus(Chardev *chr, unsigned int focus);
void mux_chr_send_all_event(Chardev *chr, QEMUChrEvent event);

Object *get_chardevs_root(void);

#endif /* CHARDEV_INTERNAL_H */
