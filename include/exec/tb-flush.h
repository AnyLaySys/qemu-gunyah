#ifndef _TB_FLUSH_H_
#define _TB_FLUSH_H_

void tb_flush(CPUState *cs);

void tcg_flush_jmp_cache(CPUState *cs);

#endif /* _TB_FLUSH_H_ */
