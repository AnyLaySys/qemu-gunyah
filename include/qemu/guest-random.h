
#ifndef QEMU_GUEST_RANDOM_H
#define QEMU_GUEST_RANDOM_H

int qemu_guest_random_seed_main(const char *seedstr, Error **errp);

uint64_t qemu_guest_random_seed_thread_part1(void);

void qemu_guest_random_seed_thread_part2(uint64_t seed);

int qemu_guest_getrandom(void *buf, size_t len, Error **errp);

void qemu_guest_getrandom_nofail(void *buf, size_t len);

#endif /* QEMU_GUEST_RANDOM_H */
