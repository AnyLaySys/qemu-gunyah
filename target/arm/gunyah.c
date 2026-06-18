/*
 * QEMU Gunyah hypervisor support
 *
 * Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

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

/*
 * Specify location of device-tree in guest address space.
 *
 * @dtb_start - Guest physical address where VM's device-tree is found
 * @dtb_size - Size of device-tree (and any free space after it).
 *
 * RM or Resource Manager VM is a trusted and privileged VM that works in
 * collaboration with Gunyah hypevisor to setup resources for a VM before it can
 * begin execution. One of its functions includes inspection/modification of a
 * VM's device-tree before VM begins its execution. Modification can
 * include specification of runtime resources allocated by hypervisor,
 * details of which needs to be visible to VM.  VM's device-tree is modified
 * "inline" making use of "free" space that could exist at the end of device
 * tree.
 */
int gunyah_arm_set_dtb(uint64_t dtb_start, uint64_t dtb_size)
{
    GUNYAHState *state = get_gunyah_state();

    /*
     * Store DTB location for gunyah_start_vm() to use later.
     * The actual GH_VM_SET_DTB_CONFIG ioctl is deferred to gunyah_start_vm()
     * so that the ioctl ordering matches CrosVM:
     *   memory → VCPU → DTB → boot_ctx → VM_START
     */
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
    /*
     * Set base-address to 0 instead of mem_base (0x80000000).
     *
     * CrosVM uses base-address=PHYS_MEM_START (0x80000000) because it
     * uses virtio over shared memory for all I/O — no MMIO devices
     * below the RAM base.
     *
     * QEMU's virt machine places device MMIO at low addresses:
     *   GIC:    0x08000000
     *   UART:   0x09000000
     *   RTC:    0x09010000
     *   virtio: 0x0a000000
     *
     * Gunyah only generates MMIO exits (GH_VCPU_EXIT_MMIO) for guest
     * accesses within the VM's address layout [base-address, base+size).
     * Accesses OUTSIDE the layout cause stage-2 aborts injected back
     * into the guest, which hangs the kernel during early boot.
     *
     * By setting base-address=0, we include all device addresses in
     * the VM's IPA layout, so guest device accesses trigger MMIO exits
     * back to QEMU for proper emulation.
     */
    qemu_fdt_setprop_u64(fdt, nodename, "base-address", 0);
    gh_report("gunyah-vm-config/memory base-address=0x0 "
                 "(includes QEMU device I/O below 0x%"PRIx64")", mem_base);
    /*
     * Max VM size — must cover the entire IPA range from base-address (0)
     * to the top of RAM (including swiotlb).  Round up to the next GiB
     * boundary for alignment.
     *
     * With base-address=0 and RAM at mem_base:
     *   size-max >= mem_base + ram_size
     *
     * For example, -m 4096 with RAM at 0x80000000:
     *   size-max >= 0x80000000 + 0x100000000 = 0x180000000
     */
    {
        uint64_t ram_size = current_machine->ram_size;
        uint64_t ram_top = mem_base + ram_size;
        /* Round up to next GiB, minimum 4 GiB */
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

    /*
     * SHM vdevice nodes: tell RM about shared memory regions.
     * Format matches CrosVM (not original qemu-gunyah which has
     * push-compatible="dma" that CrosVM doesn't use).
     *
     * CrosVM format:
     *   shm-<idx> { vdevice-type="shm"; peer-default; dma_base=<0>;
     *     memory { optional; label=<idx>; #address-cells=<2>; base=<gpa>; };
     *   };
     */
    for (i = 0; i < state->nr_slots; ++i) {
        if (!state->slots[i].start || state->slots[i].lend ||
                state->slots[i].start == mem_base ||
                state->slots[i].start < (1ULL << 30)) {
            /*
             * Skip: zero-start, LEND'd, main RAM, or pflash/ROM regions
             * below 1 GiB.  SHM vdevice nodes are for shared memory
             * buffers (like swiotlb), not firmware flash.  Creating SHM
             * nodes for pflash confuses the Resource Manager.
             */
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

    /* SimpleFB DTB node is created by the simplefb device itself,
     * not here — avoids FDT_ERR_EXISTS conflict. */

    /*
     * Doorbell vdevice nodes.
     * Base doorbells matching CrosVM:
     *   bell-0: SPI 0 EDGE_RISING  (serial)
     *   bell-1: SPI 1 EDGE_RISING  (RTC)
     *   bell-2: SPI 2 EDGE_RISING  (serial)
     *   bell-f: SPI 15 EDGE_RISING (vmwdt)
     * PCI INTx doorbells (QEMU virt irqmap[VIRT_PCIE]=3, SPIs 3-6):
     *   bell-3: SPI 3 LEVEL_HIGH   (PCI INTA)
     *   bell-4: SPI 4 LEVEL_HIGH   (PCI INTB, also in CrosVM)
     *   bell-5: SPI 5 LEVEL_HIGH   (PCI INTC)
     *   bell-6: SPI 6 LEVEL_HIGH   (PCI INTD)
     */
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

        gh_report("creating %d doorbell DTB nodes (matching CrosVM)",
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

    /*
     * Doorbell vdevice nodes for virtio-mmio IRQs.
     * Labels 0x10-0x2f map to SPIs 16-47, matching QEMU virt's
     * virtio-mmio IRQ assignment.  EDGE_RISING trigger.
     */
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

/*
 * SimpleFB: carve 8MB from the end of the SHARE'd (SWIOTLB) region for a
 * framebuffer.  This gives the guest a physical address that the host can
 * also read, solving the LEND'd-memory display problem.
 *
 * Memory layout (end of guest RAM):
 *   ... LEND'd main RAM ...
 *   [simplefb 8MB] [SWIOTLB remaining] ← both SHARE'd
 *
 * The simplefb address = mem_base + ram_size - swiotlb_size
 * (first 8MB of the SHARE'd region)
 */
#define GUNYAH_SIMPLEFB_SIZE  (8 * 1024 * 1024)  /* 8MB, enough for 1920x1080x4 */

uint64_t gunyah_get_simplefb_addr(void)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    GUNYAHState *s = get_gunyah_state();
    uint64_t mem_base = 0x80000000;  /* AARCH64_PHYS_MEM_START */
    return mem_base + ms->ram_size - s->swiotlb_size;
}

uint64_t gunyah_get_simplefb_size(void)
{
    return GUNYAH_SIMPLEFB_SIZE;
}

int gunyah_arch_put_registers(CPUState *cs, int level)
{
    /*
     * No support (yet) to set/get vCPU registers.
     *
     * We specify device-tree location via GH_VM_SET_DTB_CONFIG, which is
     * passed on to RM. RM will inspect the device-tree before VM begins
     * execution and makes arrangement (with hypervisor) for VM to receive a
     * pointer to device-tree via X0 register at boot time.
     *
     * Image entry point is assumed to be the beginning of slot containing
     * device-tree, which seems to be true currently. In future, Gunyah could
     * add API to set boot CPU's register context.
     */

    return 0;
}
