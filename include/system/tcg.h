

#ifndef SYSTEM_TCG_H
#define SYSTEM_TCG_H

#ifdef CONFIG_TCG
extern bool tcg_allowed;
#define tcg_enabled() (tcg_allowed)
#else
#define tcg_enabled() 0
#endif

#endif
