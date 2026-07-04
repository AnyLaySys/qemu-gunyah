
#ifndef HW_SERIAL_H
#define HW_SERIAL_H

#include "chardev/char-fe.h"
#include "exec/memory.h"
#include "qemu/fifo8.h"
#include "qom/object.h"

#define UART_FIFO_LENGTH    16      /* 16550A Fifo Length */

struct SerialState {
    DeviceState parent;

    uint16_t divider;
    uint8_t rbr; /* receive register */
    uint8_t thr; /* transmit holding register */
    uint8_t tsr; /* transmit shift register */
    uint8_t ier;
    uint8_t iir; /* read only */
    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr; /* read only */
    uint8_t msr; /* read only */
    uint8_t scr;
    uint8_t fcr;
    uint8_t fcr_vmstate; /* we can't write directly this value
                            it has side effects */
    int thr_ipending;
    qemu_irq irq;
    CharBackend chr;
    int last_break_enable;
    uint32_t baudbase;
    uint32_t tsr_retry;
    guint watch_tag;
    bool wakeup;

    uint64_t last_xmit_ts;
    Fifo8 recv_fifo;
    Fifo8 xmit_fifo;
    uint8_t recv_fifo_itl;

    QEMUTimer *fifo_timeout_timer;
    int timeout_ipending;           /* timeout interrupt pending state */

    uint64_t char_transmit_time;    /* time to transmit a char in ticks */
    int poll_msl;

    QEMUTimer *modem_status_poll;
    MemoryRegion io;
};
typedef struct SerialState SerialState;

extern const VMStateDescription vmstate_serial;
extern const MemoryRegionOps serial_io_ops;

#define TYPE_SERIAL "serial"
OBJECT_DECLARE_SIMPLE_TYPE(SerialState, SERIAL)

#endif
