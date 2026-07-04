#ifndef CHAR_SERIAL_H
#define CHAR_SERIAL_H

#include "chardev/char.h"

#define CHR_IOCTL_SERIAL_SET_PARAMS   1
typedef struct {
    int speed;
    int parity;
    int data_bits;
    int stop_bits;
} QEMUSerialSetParams;

#define CHR_IOCTL_SERIAL_SET_BREAK    2

#define CHR_IOCTL_SERIAL_SET_TIOCM   13
#define CHR_IOCTL_SERIAL_GET_TIOCM   14

#define CHR_TIOCM_CTS   0x020
#define CHR_TIOCM_CAR   0x040
#define CHR_TIOCM_DSR   0x100
#define CHR_TIOCM_RI    0x080
#define CHR_TIOCM_DTR   0x002
#define CHR_TIOCM_RTS   0x004

#endif
