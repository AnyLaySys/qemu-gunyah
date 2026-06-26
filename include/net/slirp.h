#ifndef QEMU_NET_SLIRP_H
#define QEMU_NET_SLIRP_H


#ifdef CONFIG_SLIRP

void hmp_hostfwd_add(Monitor *mon, const QDict *qdict);
void hmp_hostfwd_remove(Monitor *mon, const QDict *qdict);

void hmp_info_usernet(Monitor *mon, const QDict *qdict);

#endif

#endif /* QEMU_NET_SLIRP_H */
