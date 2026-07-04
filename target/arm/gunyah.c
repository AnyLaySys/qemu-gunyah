
#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "hw/boards.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "linux-headers/linux/gunyah.h"
#include "exec/memory.h"
#include "system/device_tree.h"
#include "hw/arm/fdt.h"

int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size)
{
    GUNYAHState *state = get_gunyah_state();

    state->dtb_start = dtb_start;
    state->dtb_size = dtb_size;

    gh_report("set_dtb gpa=0x%"PRIx64" size=0x%"PRIx64
                 " (deferred to VM start)", dtb_start, dtb_size);

    return 0;
}

void gunyah_arm_fdt_customize(void *fdt, uint64_t mem_base,
            uint32_t gic_phandle)
{
    char *nodename;
    int i;
    GUNYAHState *state = get_gunyah_state();

    qemu_fdt_add_subnode(fdt, "/gunyah-vm-config");
    qemu_fdt_setprop_string(fdt, "/gunyah-vm-config",
                                "image-name", "crosvm-vm");
    qemu_fdt_setprop_string(fdt, "/gunyah-vm-config", "os-type", "linux");

    nodename = g_strdup_printf("/gunyah-vm-config/memory");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "#address-cells", 2);
    qemu_fdt_setprop_cell(fdt, nodename, "#size-cells", 2);
    qemu_fdt_setprop_u64(fdt, nodename, "base-address", 0);
    gh_report("gunyah-vm-config/memory base-address=0x0 "
                 "(includes QEMU device I/O below 0x%"PRIx64")", mem_base);
    {
        uint64_t ram_size = current_machine->ram_size;
        uint64_t ram_top = mem_base + ram_size;
        uint64_t size_max = ROUND_UP(ram_top, GiB);
        if (size_max < 4 * GiB) {
            size_max = 4 * GiB;
        }
        qemu_fdt_setprop_u64(fdt, nodename, "size-max", size_max);
        gh_report("gunyah-vm-config/memory size-max=0x%"PRIx64
                     " (ram_top=0x%"PRIx64")", size_max, ram_top);
    }

    g_free(nodename);

    nodename = g_strdup_printf("/gunyah-vm-config/interrupts");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_cell(fdt, nodename, "config", gic_phandle);
    g_free(nodename);

    nodename = g_strdup_printf("/gunyah-vm-config/vcpus");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "affinity", "proxy");
    g_free(nodename);

    nodename = g_strdup_printf("/gunyah-vm-config/vdevices");
    qemu_fdt_add_subnode(fdt, nodename);
    qemu_fdt_setprop_string(fdt, nodename, "generate", "/hypervisor");
    g_free(nodename);

    for (i = 0; i < state->nr_slots; ++i) {
        if (!state->slots[i].start || state->slots[i].lend ||
                state->slots[i].start == mem_base ||
                state->slots[i].start < (1ULL << 30)) {
            continue;
        }

        gh_report("creating SHM node shm-%x gpa=0x%"PRIx64
                     " size=0x%"PRIx64, i, state->slots[i].start,
                     state->slots[i].size);

        nodename = g_strdup_printf("/gunyah-vm-config/vdevices/shm-%x", i);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop_string(fdt, nodename, "vdevice-type", "shm");
        qemu_fdt_setprop(fdt, nodename, "peer-default", NULL, 0);
        qemu_fdt_setprop_u64(fdt, nodename, "dma_base", 0);
        g_free(nodename);

        nodename = g_strdup_printf("/gunyah-vm-config/vdevices/shm-%x/memory",
                                                                        i);
        qemu_fdt_add_subnode(fdt, nodename);
        qemu_fdt_setprop(fdt, nodename, "optional", NULL, 0);
        qemu_fdt_setprop_cell(fdt, nodename, "label", i);
        qemu_fdt_setprop_cell(fdt, nodename, "#address-cells", 2);
        qemu_fdt_setprop_u64(fdt, nodename, "base", state->slots[i].start);
        g_free(nodename);
    }


    {
        struct { int label; int spi; int flags; } bells[] = {
            { 0x0, 0x0, 0x01 },  /* EDGE_RISING */
            { 0x1, 0x1, 0x04 },  /* LEVEL_HIGH - PL011 UART */
            { 0x2, 0x2, 0x01 },  /* EDGE_RISING */
            { 0x3, 0x3, 0x04 },  /* LEVEL_HIGH - PCI INTA */
            { 0x4, 0x4, 0x04 },  /* LEVEL_HIGH - PCI INTB */
            { 0x5, 0x5, 0x04 },  /* LEVEL_HIGH - PCI INTC */
            { 0x6, 0x6, 0x04 },  /* LEVEL_HIGH - PCI INTD */
            { 0xf, 0xf, 0x01 },  /* EDGE_RISING */
        };
        int nbell = sizeof(bells) / sizeof(bells[0]);

        gh_report("creating %d doorbell DTB nodes",
                     nbell);

        for (i = 0; i < nbell; ++i) {
            char *p;
            nodename = g_strdup_printf(
                "/gunyah-vm-config/vdevices/bell-%x", bells[i].label);
            qemu_fdt_add_subnode(fdt, nodename);
            qemu_fdt_setprop_string(fdt, nodename,
                                    "vdevice-type", "doorbell");
            p = g_strdup_printf("/hypervisor/bell-%x", bells[i].label);
            qemu_fdt_setprop_string(fdt, nodename, "generate", p);
            g_free(p);
            qemu_fdt_setprop_cell(fdt, nodename, "label", bells[i].label);
            qemu_fdt_setprop(fdt, nodename, "peer-default", NULL, 0);
            qemu_fdt_setprop(fdt, nodename, "source-can-clear", NULL, 0);

            qemu_fdt_setprop_cells(fdt, nodename, "interrupts",
                    GIC_FDT_IRQ_TYPE_SPI, bells[i].spi, bells[i].flags);

            g_free(nodename);
        }
    }

    {
        gh_report("creating 32 virtio doorbell DTB nodes "
                     "(labels 0x10-0x2f, SPIs 16-47)");
        for (i = 0; i < 32; i++) {
            char *p;
            int label = 0x10 + i;
            int spi = 16 + i;

            nodename = g_strdup_printf(
                "/gunyah-vm-config/vdevices/bell-%x", label);
            qemu_fdt_add_subnode(fdt, nodename);
            qemu_fdt_setprop_string(fdt, nodename,
                                    "vdevice-type", "doorbell");
            p = g_strdup_printf("/hypervisor/bell-%x", label);
            qemu_fdt_setprop_string(fdt, nodename, "generate", p);
            g_free(p);
            qemu_fdt_setprop_cell(fdt, nodename, "label", label);
            qemu_fdt_setprop(fdt, nodename, "peer-default", NULL, 0);
            qemu_fdt_setprop(fdt, nodename, "source-can-clear", NULL, 0);

            qemu_fdt_setprop_cells(fdt, nodename, "interrupts",
                    GIC_FDT_IRQ_TYPE_SPI, spi, 0x01 /* EDGE_RISING */);

            g_free(nodename);
        }
    }
}

int gunyah_arch_put_registers(CPUState *cs, int level)
{

    return 0;
}
