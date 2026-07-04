
#ifndef HOST_CPUINFO_H
#define HOST_CPUINFO_H

#define CPUINFO_ALWAYS          (1u << 0)  /* so cpuinfo is nonzero */
#define CPUINFO_LSE             (1u << 1)
#define CPUINFO_LSE2            (1u << 2)
#define CPUINFO_AES             (1u << 3)
#define CPUINFO_PMULL           (1u << 4)
#define CPUINFO_BTI             (1u << 5)

extern unsigned cpuinfo;

unsigned cpuinfo_init(void);

#endif /* HOST_CPUINFO_H */
