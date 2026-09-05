
#include "qemu/osdep.h"
#include <libfdt.h>
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qemu/cutils.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "linux-headers/linux/gunyah.h"
#include "exec/memory.h"
#include "system/device_tree.h"
#include "hw/arm/fdt.h"
#include "hw/intc/arm_gic_common.h"

int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size)
{
    GUNYAHState *state = get_gunyah_state();

    state->dtb_start = dtb_start;
    state->dtb_size = dtb_size;

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
    {
        uint64_t ram_size = current_machine->ram_size;
        uint64_t ram_top = mem_base + ram_size;
        uint64_t size_max = ROUND_UP(ram_top, GiB);
        if (size_max < 4 * GiB) {
            size_max = 4 * GiB;
        }
        qemu_fdt_setprop_u64(fdt, nodename, "size-max", size_max);
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
            { 0x0, 0x0, 0x01 },
            { 0x1, 0x1, 0x04 },
            { 0x2, 0x2, 0x01 },
            { 0xf, 0xf, 0x01 },
        };
        int nbell = sizeof(bells) / sizeof(bells[0]);

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
        for (i = 0; i < state->msi_vectors; i++) {
            char *p;
            int label = GUNYAH_MSI_SPI_BASE + i;
            int spi = GUNYAH_MSI_SPI_BASE + i;

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
                    GIC_FDT_IRQ_TYPE_SPI, spi, 0x01);

            g_free(nodename);
        }
    }
}

void gunyah_arm_build_dtb(const struct arm_boot_info *binfo, void *fdt)
{
    MachineState *ms = current_machine;
    GUNYAHState *gs = get_gunyah_state();
    uint64_t mem_base = binfo->loader_start;
    uint64_t mem_size = ms->ram_size;
    uint64_t guest_mem_size = mem_size;
    int ret;

    ret = fdt_create_empty_tree(fdt, 0x100000);
    if (ret) {
        error_report("fdt_create_empty_tree failed: %s", fdt_strerror(ret));
        exit(1);
    }

    fdt_setprop_string(fdt, 0, "compatible", "linux,dummy-virt");
    fdt_setprop_string(fdt, 0, "model", "QEMU Gunyah Virtual Machine");
    fdt_setprop_cell(fdt, 0, "interrupt-parent", 1);
    fdt_setprop_cell(fdt, 0, "#address-cells", 2);
    fdt_setprop_cell(fdt, 0, "#size-cells", 2);

    {
        const char *earlycon = "earlycon=pl011,mmio32,0x09000000 console=ttyAMA0";
        const char *cmdline = binfo->kernel_cmdline;
        char bootargs[1024];
        int node = fdt_add_subnode(fdt, 0, "chosen");

        if (cmdline && *cmdline) {
            snprintf(bootargs, sizeof(bootargs), "%s %s", earlycon, cmdline);
        } else {
            pstrcpy(bootargs, sizeof(bootargs), earlycon);
        }
        fdt_setprop_string(fdt, node, "bootargs", bootargs);
        fdt_setprop_string(fdt, node, "stdout-path", "/pl011@9000000");
    }

    {
        int node = fdt_add_subnode(fdt, 0, "aliases");
        fdt_setprop_string(fdt, node, "serial0", "/pl011@9000000");
    }

    {
        int node = fdt_add_subnode(fdt, 0, "config");
        fdt_setprop_cell(fdt, node, "kernel-address", mem_base);
        fdt_setprop_cell(fdt, node, "kernel-size", 0x1000000);
    }

    if (gs->swiotlb_size) {
        guest_mem_size -= gs->swiotlb_size;
    }
    {
        fdt64_t reg[] = {
            cpu_to_fdt64(mem_base),
            cpu_to_fdt64(guest_mem_size),
        };
        int node = fdt_add_subnode(fdt, 0, "memory");

        fdt_setprop_string(fdt, node, "device_type", "memory");
        fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
    }

    {
        int cpus = fdt_add_subnode(fdt, 0, "cpus");
        int i;

        fdt_setprop_cell(fdt, cpus, "#address-cells", 1);
        fdt_setprop_cell(fdt, cpus, "#size-cells", 0);
        for (i = 0; i < ms->smp.cpus; i++) {
            char name[32];
            int cpu;

            snprintf(name, sizeof(name), "cpu@%d", i);
            cpu = fdt_add_subnode(fdt, cpus, name);
            fdt_setprop_string(fdt, cpu, "device_type", "cpu");
            fdt_setprop_string(fdt, cpu, "compatible", "arm,armv8");
            fdt_setprop_cell(fdt, cpu, "reg", i);
            fdt_setprop_cell(fdt, cpu, "phandle", 0x100 + i);
            if (ms->smp.cpus > 1) {
                fdt_setprop_string(fdt, cpu, "enable-method", "psci");
            }
        }
    }

    {
        uint32_t redist_size = 0x20000 * ms->smp.cpus;
        fdt64_t reg[] = {
            cpu_to_fdt64(0x08000000), cpu_to_fdt64(0x10000),
            cpu_to_fdt64(0x080a0000), cpu_to_fdt64(redist_size),
        };
        int node = fdt_add_subnode(fdt, 0, "intc");

        fdt_setprop_string(fdt, node, "compatible", "arm,gic-v3");
        fdt_setprop_cell(fdt, node, "#interrupt-cells", 3);
        fdt_setprop(fdt, node, "interrupt-controller", NULL, 0);
        fdt_setprop_cell(fdt, node, "#address-cells", 2);
        fdt_setprop_cell(fdt, node, "#size-cells", 2);
        fdt_setprop(fdt, node, "ranges", NULL, 0);
        fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
        fdt_setprop_cell(fdt, node, "phandle", 1);
        {
            fdt64_t v2m_reg[] = {
                cpu_to_fdt64(GUNYAH_V2M_BASE),
                cpu_to_fdt64(GUNYAH_V2M_SIZE),
            };
            int v2m = fdt_add_subnode(fdt, node, "v2m@8020000");

            fdt_setprop_string(fdt, v2m, "compatible", "arm,gic-v2m-frame");
            fdt_setprop(fdt, v2m, "msi-controller", NULL, 0);
            fdt_setprop(fdt, v2m, "reg", v2m_reg, sizeof(v2m_reg));
            fdt_setprop_cell(fdt, v2m, "arm,msi-base-spi",
                             GUNYAH_MSI_SPI_BASE + GIC_INTERNAL);
            fdt_setprop_cell(fdt, v2m, "arm,msi-num-spis",
                             gs->msi_vectors);
            fdt_setprop_cell(fdt, v2m, "phandle", GUNYAH_MSI_PHANDLE);
        }
    }

    {
        fdt32_t interrupts[] = {
            cpu_to_fdt32(1), cpu_to_fdt32(13), cpu_to_fdt32(0x108),
            cpu_to_fdt32(1), cpu_to_fdt32(14), cpu_to_fdt32(0x108),
            cpu_to_fdt32(1), cpu_to_fdt32(11), cpu_to_fdt32(0x108),
            cpu_to_fdt32(1), cpu_to_fdt32(10), cpu_to_fdt32(0x108),
        };
        int node = fdt_add_subnode(fdt, 0, "timer");

        fdt_setprop_string(fdt, node, "compatible", "arm,armv8-timer");
        fdt_setprop(fdt, node, "interrupts", interrupts, sizeof(interrupts));
        fdt_setprop(fdt, node, "always-on", NULL, 0);
    }

    {
        int node = fdt_add_subnode(fdt, 0, "psci");
        fdt_setprop_string(fdt, node, "compatible", "arm,psci-0.2");
        fdt_setprop_string(fdt, node, "method", "hvc");
    }

    if (gs->swiotlb_size) {
        uint64_t start = mem_base + mem_size - gs->swiotlb_size;
        fdt64_t reg[] = {
            cpu_to_fdt64(start),
            cpu_to_fdt64(gs->swiotlb_size),
        };
        fdt64_t alignment = cpu_to_fdt64(0x1000);
        char name[64];
        int reserved = fdt_add_subnode(fdt, 0, "reserved-memory");
        int pool;

        fdt_setprop_cell(fdt, reserved, "#address-cells", 2);
        fdt_setprop_cell(fdt, reserved, "#size-cells", 2);
        fdt_setprop(fdt, reserved, "ranges", NULL, 0);
        snprintf(name, sizeof(name), "restricted_dma_reserved@%" PRIx64,
                 start);
        pool = fdt_add_subnode(fdt, reserved, name);
        fdt_setprop(fdt, pool, "reg", reg, sizeof(reg));
        fdt_setprop_string(fdt, pool, "compatible", "restricted-dma-pool");
        fdt_setprop(fdt, pool, "alignment", &alignment, sizeof(alignment));
        fdt_setprop_cell(fdt, pool, "phandle", 2);
    }

    {
        const uint64_t ecam = 0x3f000000;
        const uint64_t ecam_size = 0x01000000;
        const uint64_t mmio = 0x10000000;
        const uint64_t mmio_size = 0x2eff0000;
        const uint64_t pio = 0x3eff0000;
        const uint64_t pio_size = 0x00010000;
        fdt64_t reg[] = { cpu_to_fdt64(ecam), cpu_to_fdt64(ecam_size) };
        fdt32_t bus_range[] = { cpu_to_fdt32(0), cpu_to_fdt32(0) };
        fdt32_t ranges[14];
        fdt32_t msi_map[] = {
            cpu_to_fdt32(0), cpu_to_fdt32(GUNYAH_MSI_PHANDLE),
            cpu_to_fdt32(0), cpu_to_fdt32(0x10000),
        };
        int node = fdt_add_subnode(fdt, 0, "pcie@10000000");
        int index = 0;

        fdt_setprop_string(fdt, node, "compatible", "pci-host-ecam-generic");
        fdt_setprop_string(fdt, node, "device_type", "pci");
        fdt_setprop_cell(fdt, node, "#address-cells", 3);
        fdt_setprop_cell(fdt, node, "#size-cells", 2);
        fdt_setprop_cell(fdt, node, "linux,pci-domain", 0);
        fdt_setprop(fdt, node, "bus-range", bus_range, sizeof(bus_range));
        fdt_setprop(fdt, node, "dma-coherent", NULL, 0);
        fdt_setprop(fdt, node, "msi-map", msi_map, sizeof(msi_map));
        fdt_setprop(fdt, node, "reg", reg, sizeof(reg));

        ranges[index++] = cpu_to_fdt32(0x01000000);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(pio);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(pio_size);
        ranges[index++] = cpu_to_fdt32(0x02000000);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(mmio);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(mmio);
        ranges[index++] = cpu_to_fdt32(0);
        ranges[index++] = cpu_to_fdt32(mmio_size);
        fdt_setprop(fdt, node, "ranges", ranges, sizeof(ranges));
        if (gs->swiotlb_size) {
            fdt_setprop_cell(fdt, node, "memory-region", 2);
        }
    }

    {
        int node = fdt_add_subnode(fdt, 0, "apb-pclk");
        fdt_setprop_string(fdt, node, "compatible", "fixed-clock");
        fdt_setprop_cell(fdt, node, "#clock-cells", 0);
        fdt_setprop_cell(fdt, node, "clock-frequency", 24000000);
        fdt_setprop_string(fdt, node, "clock-output-names", "clk24mhz");
        fdt_setprop_cell(fdt, node, "phandle", 3);
    }

    {
        const char compatible[] = "arm,pl011\0arm,primecell";
        const char clock_names[] = "uartclk\0apb_pclk";
        fdt64_t reg[] = { cpu_to_fdt64(0x09000000), cpu_to_fdt64(0x1000) };
        fdt32_t irq[] = { cpu_to_fdt32(0), cpu_to_fdt32(1), cpu_to_fdt32(4) };
        fdt32_t clocks[] = { cpu_to_fdt32(3), cpu_to_fdt32(3) };
        int node = fdt_add_subnode(fdt, 0, "pl011@9000000");

        fdt_setprop(fdt, node, "compatible", compatible, sizeof(compatible));
        fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
        fdt_setprop(fdt, node, "interrupts", irq, sizeof(irq));
        fdt_setprop(fdt, node, "clocks", clocks, sizeof(clocks));
        fdt_setprop(fdt, node, "clock-names", clock_names,
                    sizeof(clock_names));
        fdt_setprop_cell(fdt, node, "clock-frequency", 24000000);
        fdt_setprop(fdt, node, "dma-coherent", NULL, 0);
        if (gs->swiotlb_size) {
            fdt_setprop_cell(fdt, node, "memory-region", 2);
        }
    }

    {
        fdt64_t reg[] = { cpu_to_fdt64(0x09020000), cpu_to_fdt64(0x10) };
        int node = fdt_add_subnode(fdt, 0, "fw-cfg@9020000");

        fdt_setprop_string(fdt, node, "compatible", "qemu,fw-cfg-mmio");
        fdt_setprop(fdt, node, "reg", reg, sizeof(reg));
        fdt_setprop(fdt, node, "dma-coherent", NULL, 0);
    }

    gunyah_arm_fdt_customize(fdt, mem_base, 1);
    {
        int node = fdt_add_subnode(fdt, 0, "__symbols__");
        fdt_setprop_string(fdt, node, "intc", "/intc");
    }
}

int gunyah_arch_put_registers(CPUState *cs, int level)
{

    return 0;
}
