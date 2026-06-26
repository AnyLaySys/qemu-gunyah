
#ifndef QEMU_SYSTEMD_H
#define QEMU_SYSTEMD_H

#define FIRST_SOCKET_ACTIVATION_FD 3 /* defined by systemd ABI */

unsigned int check_socket_activation(void);

#endif
