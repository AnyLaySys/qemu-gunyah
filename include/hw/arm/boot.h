
#ifndef HW_ARM_BOOT_H
#define HW_ARM_BOOT_H

#include "target/arm/cpu-qom.h"
#include "qemu/notify.h"

typedef enum {
    ARM_ENDIANNESS_UNKNOWN = 0,
    ARM_ENDIANNESS_LE,
    ARM_ENDIANNESS_BE8,
    ARM_ENDIANNESS_BE32,
} arm_endianness;

void armv7m_load_kernel(ARMCPU *cpu, const char *kernel_filename,
                        hwaddr mem_base, int mem_size);

struct arm_boot_info {
    uint64_t ram_size;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
    const char *dtb_filename;
    hwaddr loader_start;
    hwaddr dtb_start;
    hwaddr dtb_limit;
    bool skip_dtb_autoload;
    hwaddr smp_loader_start;
    hwaddr smp_bootreg_addr;
    hwaddr gic_cpu_if_addr;
    int board_id;
    bool secure_boot;
    int (*atag_board)(const struct arm_boot_info *info, void *p);
    void (*write_secondary_boot)(ARMCPU *cpu,
                                 const struct arm_boot_info *info);
    void (*secondary_cpu_reset_hook)(ARMCPU *cpu,
                                     const struct arm_boot_info *info);
    void *(*get_dtb)(const struct arm_boot_info *info, int *size);
    void (*modify_dtb)(const struct arm_boot_info *info, void *fdt);
    int psci_conduit;
    int is_linux;
    hwaddr initrd_start;
    hwaddr initrd_size;
    hwaddr entry;

    bool firmware_loaded;

    hwaddr board_setup_addr;
    void (*write_board_setup)(ARMCPU *cpu,
                              const struct arm_boot_info *info);

    bool secure_board_setup;

    arm_endianness endianness;
};

void arm_load_kernel(ARMCPU *cpu, MachineState *ms, struct arm_boot_info *info);

AddressSpace *arm_boot_address_space(ARMCPU *cpu,
                                     const struct arm_boot_info *info);

int arm_load_dtb(hwaddr addr, const struct arm_boot_info *binfo,
                 hwaddr addr_limit, AddressSpace *as, MachineState *ms,
                 ARMCPU *cpu);

void arm_write_secure_board_setup_dummy_smc(ARMCPU *cpu,
                                            const struct arm_boot_info *info,
                                            hwaddr mvbar_addr);

typedef enum {
    FIXUP_NONE = 0,     /* do nothing */
    FIXUP_TERMINATOR,   /* end of insns */
    FIXUP_BOARDID,      /* overwrite with board ID number */
    FIXUP_BOARD_SETUP,  /* overwrite with board specific setup code address */
    FIXUP_ARGPTR_LO,    /* overwrite with pointer to kernel args */
    FIXUP_ARGPTR_HI,    /* overwrite with pointer to kernel args (high half) */
    FIXUP_ENTRYPOINT_LO, /* overwrite with kernel entry point */
    FIXUP_ENTRYPOINT_HI, /* overwrite with kernel entry point (high half) */
    FIXUP_GIC_CPU_IF,   /* overwrite with GIC CPU interface address */
    FIXUP_BOOTREG,      /* overwrite with boot register address */
    FIXUP_DSB,          /* overwrite with correct DSB insn for cpu */
    FIXUP_MAX,
} FixupType;

typedef struct ARMInsnFixup {
    uint32_t insn;
    FixupType fixup;
} ARMInsnFixup;

void arm_write_bootloader(const char *name,
                          AddressSpace *as, hwaddr addr,
                          const ARMInsnFixup *insns,
                          const uint32_t *fixupcontext);

#endif /* HW_ARM_BOOT_H */
