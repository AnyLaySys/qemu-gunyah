
#ifndef TCG_STARTUP_H
#define TCG_STARTUP_H

void tcg_init(size_t tb_size, int splitwx, unsigned max_cpus);

void tcg_register_thread(void);

void tcg_prologue_init(void);

#endif
