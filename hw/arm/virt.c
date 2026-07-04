

#include "qemu/osdep.h"
#include <sys/stat.h>
#include "qemu/datadir.h"
#include "qemu/units.h"
#include "qemu/option.h"
#include "monitor/qdev.h"
#include "hw/sysbus.h"
#include "hw/arm/boot.h"
#include "hw/arm/primecell.h"
#include "hw/arm/virt.h"
#include "hw/char/serial-mm.h"
#include "net/net.h"
#include "system/device_tree.h"
#include <libfdt.h>
#include "system/numa.h"
#include "system/runstate.h"
#include "system/system.h"
#include "system/tcg.h"
#include "system/hvf.h"
#include "system/gunyah.h"
#include "system/gunyah_int.h"
#include "system/confidential-guest-support.h"
#include "qom/object_interfaces.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "qemu/bitops.h"
#include "target/arm/cpu.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "hw/pci-host/gpex.h"
#include "hw/virtio/virtio-pci.h"
#include "hw/core/sysbus-fdt.h"
#include "hw/platform-bus.h"
#include "hw/qdev-properties.h"
#include "hw/arm/fdt.h"
#include "hw/intc/arm_gic.h"
#include "hw/intc/arm_gicv3_common.h"
#include "hw/intc/arm_gicv3_its_common.h"
#include "hw/irq.h"
#include "hvf_arm.h"
#include "qapi/visitor.h"
#include "qapi/qapi-visit-common.h"
#include "qobject/qlist.h"
#include "standard-headers/linux/input.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/internals.h"
#include "target/arm/multiprocessing.h"
#include "target/arm/gtimer.h"
#include "hw/virtio/virtio-iommu.h"
#include "hw/char/pl011.h"
#include "qemu/guest-random.h"

static GlobalProperty arm_virt_compat[] = {
 { TYPE_VIRTIO_IOMMU_PCI, "aw-bits", "48" },
};
static const size_t arm_virt_compat_len = G_N_ELEMENTS(arm_virt_compat);

static void arm_virt_compat_set(MachineClass *mc)
{
 compat_props_add(mc->compat_props, arm_virt_compat,
 arm_virt_compat_len);
}

#define DEFINE_VIRT_MACHINE_IMPL(latest, ...) \
 static void MACHINE_VER_SYM(class_init, virt, __VA_ARGS__)( \
 ObjectClass *oc, \
 void *data) \
 { \
 MachineClass *mc = MACHINE_CLASS(oc); \
 arm_virt_compat_set(mc); \
 MACHINE_VER_SYM(options, virt, __VA_ARGS__)(mc); \
 mc->desc = "QEMU " MACHINE_VER_STR(__VA_ARGS__) " ARM Virtual Machine"; \
 MACHINE_VER_DEPRECATION(__VA_ARGS__); \
 if (latest) { \
 mc->alias = "virt"; \
 } \
 } \
 static const TypeInfo MACHINE_VER_SYM(info, virt, __VA_ARGS__) = \
 { \
 .name = MACHINE_VER_TYPE_NAME("virt", __VA_ARGS__), \
 .parent = TYPE_VIRT_MACHINE, \
 .class_init = MACHINE_VER_SYM(class_init, virt, __VA_ARGS__), \
 }; \
 static void MACHINE_VER_SYM(register, virt, __VA_ARGS__)(void) \
 { \
 MACHINE_VER_DELETION(__VA_ARGS__); \
 type_register_static(&MACHINE_VER_SYM(info, virt, __VA_ARGS__)); \
 } \
 type_init(MACHINE_VER_SYM(register, virt, __VA_ARGS__));

#define DEFINE_VIRT_MACHINE_AS_LATEST(major, minor) \
 DEFINE_VIRT_MACHINE_IMPL(true, major, minor)
#define DEFINE_VIRT_MACHINE(major, minor) \
 DEFINE_VIRT_MACHINE_IMPL(false, major, minor)

#define NUM_IRQS 256

#define PLATFORM_BUS_NUM_IRQS 64

#define LEGACY_RAMLIMIT_GB 255
#define LEGACY_RAMLIMIT_BYTES (LEGACY_RAMLIMIT_GB * GiB)

static const MemMapEntry base_memmap[] = {

 [VIRT_FLASH] = { 0, 0x08000000 },
 [VIRT_CPUPERIPHS] = { 0x08000000, 0x00020000 },

 [VIRT_GIC_DIST] = { 0x08000000, 0x00010000 },
 [VIRT_GIC_CPU] = { 0x08010000, 0x00010000 },
 [VIRT_GIC_V2M] = { 0x08020000, 0x00001000 },
 [VIRT_GIC_HYP] = { 0x08030000, 0x00010000 },
 [VIRT_GIC_VCPU] = { 0x08040000, 0x00010000 },

 [VIRT_GIC_ITS] = { 0x08080000, 0x00020000 },

 [VIRT_GIC_REDIST] = { 0x080A0000, 0x00F60000 },
 [VIRT_UART0] = { 0x09000000, 0x00001000 },
 [VIRT_RTC] = { 0x09010000, 0x00001000 },
 [VIRT_FW_CFG] = { 0x09020000, 0x00000018 },
 [VIRT_GPIO] = { 0x09030000, 0x00001000 },
 [VIRT_UART1] = { 0x09040000, 0x00001000 },
 [VIRT_SMMU] = { 0x09050000, 0x00020000 },
 [VIRT_PVTIME] = { 0x090a0000, 0x00010000 },
 [VIRT_SECURE_GPIO] = { 0x090b0000, 0x00001000 },
 [VIRT_MMIO] = { 0x0a000000, 0x00000200 },

 [VIRT_PLATFORM_BUS] = { 0x0c000000, 0x02000000 },
 [VIRT_SECURE_MEM] = { 0x0e000000, 0x01000000 },
 [VIRT_PCIE_MMIO] = { 0x10000000, 0x2eff0000 },
 [VIRT_PCIE_PIO] = { 0x3eff0000, 0x00010000 },
 [VIRT_PCIE_ECAM] = { 0x3f000000, 0x01000000 },

 [VIRT_MEM] = { GiB, LEGACY_RAMLIMIT_BYTES },
};

#define DEFAULT_HIGH_PCIE_MMIO_SIZE_GB 512
#define DEFAULT_HIGH_PCIE_MMIO_SIZE (DEFAULT_HIGH_PCIE_MMIO_SIZE_GB * GiB)

static MemMapEntry extended_memmap[] = {

 [VIRT_HIGH_GIC_REDIST2] = { 0x0, 64 * MiB },
 [VIRT_HIGH_PCIE_ECAM] = { 0x0, 256 * MiB },

 [VIRT_HIGH_PCIE_MMIO] = { 0x0, DEFAULT_HIGH_PCIE_MMIO_SIZE },
};

static const int a15irqmap[] = {
 [VIRT_UART0] = 1,
 [VIRT_RTC] = 2,
 [VIRT_PCIE] = 3,
 [VIRT_GPIO] = 7,
 [VIRT_UART1] = 8,
 [VIRT_MMIO] = 16,
 [VIRT_GIC_V2M] = 48,
 [VIRT_SMMU] = 74,
 [VIRT_PLATFORM_BUS] = 112,
};

static void create_randomness(MachineState *ms, const char *node)
{
 struct {
 uint64_t kaslr;
 uint8_t rng[32];
 } seed;

 if (qemu_guest_getrandom(&seed, sizeof(seed), NULL)) {
 return;
 }
 qemu_fdt_setprop_u64(ms->fdt, node, "kaslr-seed", seed.kaslr);
 qemu_fdt_setprop(ms->fdt, node, "rng-seed", seed.rng, sizeof(seed.rng));
}

static bool ns_el2_virt_timer_present(void)
{
 ARMCPU *cpu = ARM_CPU(qemu_get_cpu(0));
 CPUARMState *env = &cpu->env;

 return arm_feature(env, ARM_FEATURE_AARCH64) &&
 arm_feature(env, ARM_FEATURE_EL2) && cpu_isar_feature(aa64_vh, cpu);
}

static void create_fdt(VirtMachineState *vms)
{
 MachineState *ms = MACHINE(vms);
 int nb_numa_nodes = ms->numa_state->num_nodes;
 void *fdt = create_device_tree(&vms->fdt_size);

 if (!fdt) {
 error_report("create_device_tree() failed");
 exit(1);
 }

 ms->fdt = fdt;

 qemu_fdt_setprop_string(fdt, "/", "compatible", "linux,dummy-virt");
 qemu_fdt_setprop_cell(fdt, "/", "#address-cells", 0x2);
 qemu_fdt_setprop_cell(fdt, "/", "#size-cells", 0x2);
 qemu_fdt_setprop_string(fdt, "/", "model", "linux,dummy-virt");

 qemu_fdt_setprop(fdt, "/", "dma-coherent", NULL, 0);

 qemu_fdt_add_subnode(fdt, "/chosen");
 if (vms->dtb_randomness) {
 create_randomness(ms, "/chosen");
 }

 if (vms->secure) {
 qemu_fdt_add_subnode(fdt, "/secure-chosen");
 if (vms->dtb_randomness) {
 create_randomness(ms, "/secure-chosen");
 }
 }

 qemu_fdt_add_subnode(fdt, "/aliases");

 vms->clock_phandle = qemu_fdt_alloc_phandle(fdt);
 qemu_fdt_add_subnode(fdt, "/apb-pclk");
 qemu_fdt_setprop_string(fdt, "/apb-pclk", "compatible", "fixed-clock");
 qemu_fdt_setprop_cell(fdt, "/apb-pclk", "#clock-cells", 0x0);
 qemu_fdt_setprop_cell(fdt, "/apb-pclk", "clock-frequency", 24000000);
 qemu_fdt_setprop_string(fdt, "/apb-pclk", "clock-output-names",
 "clk24mhz");
 qemu_fdt_setprop_cell(fdt, "/apb-pclk", "phandle", vms->clock_phandle);

 if (nb_numa_nodes > 0 && ms->numa_state->have_numa_distance) {
 int size = nb_numa_nodes * nb_numa_nodes * 3 * sizeof(uint32_t);
 uint32_t *matrix = g_malloc0(size);
 int idx, i, j;

 for (i = 0; i < nb_numa_nodes; i++) {
 for (j = 0; j < nb_numa_nodes; j++) {
 idx = (i * nb_numa_nodes + j) * 3;
 matrix[idx + 0] = cpu_to_be32(i);
 matrix[idx + 1] = cpu_to_be32(j);
 matrix[idx + 2] =
 cpu_to_be32(ms->numa_state->nodes[i].distance[j]);
 }
 }

 qemu_fdt_add_subnode(fdt, "/distance-map");
 qemu_fdt_setprop_string(fdt, "/distance-map", "compatible",
 "numa-distance-map-v1");
 qemu_fdt_setprop(fdt, "/distance-map", "distance-matrix",
 matrix, size);
 g_free(matrix);
 }
}

static void fdt_add_timer_nodes(const VirtMachineState *vms)
{

 ARMCPU *armcpu;
 VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
 uint32_t irqflags = GIC_FDT_IRQ_FLAGS_LEVEL_HI;
 MachineState *ms = MACHINE(vms);

 if (vmc->claim_edge_triggered_timers) {
 irqflags = GIC_FDT_IRQ_FLAGS_EDGE_LO_HI;
 }

 if (vms->gic_version == VIRT_GIC_VERSION_2) {
 irqflags = deposit32(irqflags, GIC_FDT_IRQ_PPI_CPU_START,
 GIC_FDT_IRQ_PPI_CPU_WIDTH,
 (1 << MACHINE(vms)->smp.cpus) - 1);
 }

 qemu_fdt_add_subnode(ms->fdt, "/timer");

 armcpu = ARM_CPU(qemu_get_cpu(0));
 if (arm_feature(&armcpu->env, ARM_FEATURE_V8)) {
 const char compat[] = "arm,armv8-timer\0arm,armv7-timer";
 qemu_fdt_setprop(ms->fdt, "/timer", "compatible",
 compat, sizeof(compat));
 } else {
 qemu_fdt_setprop_string(ms->fdt, "/timer", "compatible",
 "arm,armv7-timer");
 }
 qemu_fdt_setprop(ms->fdt, "/timer", "always-on", NULL, 0);
 if (vms->ns_el2_virt_timer_irq) {
 qemu_fdt_setprop_cells(ms->fdt, "/timer", "interrupts",
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_S_EL1_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_NS_EL1_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_VIRT_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_NS_EL2_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_NS_EL2_VIRT_IRQ), irqflags);
 } else {
 qemu_fdt_setprop_cells(ms->fdt, "/timer", "interrupts",
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_S_EL1_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_NS_EL1_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_VIRT_IRQ), irqflags,
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_TIMER_NS_EL2_IRQ), irqflags);
 }
}

static void fdt_add_cpu_nodes(const VirtMachineState *vms)
{
 int cpu;
 int addr_cells = 1;
 const MachineState *ms = MACHINE(vms);
 const VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);
 int smp_cpus = ms->smp.cpus;

 for (cpu = 0; cpu < smp_cpus; cpu++) {
 ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(cpu));

 if (arm_cpu_mp_affinity(armcpu) & ARM_AFF3_MASK) {
 addr_cells = 2;
 break;
 }
 }

 qemu_fdt_add_subnode(ms->fdt, "/cpus");
 qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#address-cells", addr_cells);
 qemu_fdt_setprop_cell(ms->fdt, "/cpus", "#size-cells", 0x0);

 for (cpu = smp_cpus - 1; cpu >= 0; cpu--) {
 char *nodename = g_strdup_printf("/cpus/cpu@%d", cpu);
 ARMCPU *armcpu = ARM_CPU(qemu_get_cpu(cpu));
 CPUState *cs = CPU(armcpu);

 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "cpu");
 qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
 armcpu->dtb_compatible);

 if (vms->psci_conduit != QEMU_PSCI_CONDUIT_DISABLED && smp_cpus > 1) {
 qemu_fdt_setprop_string(ms->fdt, nodename,
 "enable-method", "psci");
 }

 if (addr_cells == 2) {
 qemu_fdt_setprop_u64(ms->fdt, nodename, "reg",
 arm_cpu_mp_affinity(armcpu));
 } else {
 qemu_fdt_setprop_cell(ms->fdt, nodename, "reg",
 arm_cpu_mp_affinity(armcpu));
 }

 if (ms->possible_cpus->cpus[cs->cpu_index].props.has_node_id) {
 qemu_fdt_setprop_cell(ms->fdt, nodename, "numa-node-id",
 ms->possible_cpus->cpus[cs->cpu_index].props.node_id);
 }

 if (!vmc->no_cpu_topology) {
 qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle",
 qemu_fdt_alloc_phandle(ms->fdt));
 }

 g_free(nodename);
 }

 if (!vmc->no_cpu_topology) {

 qemu_fdt_add_subnode(ms->fdt, "/cpus/cpu-map");

 for (cpu = smp_cpus - 1; cpu >= 0; cpu--) {
 char *cpu_path = g_strdup_printf("/cpus/cpu@%d", cpu);
 char *map_path;

 if (ms->smp.threads > 1) {
 map_path = g_strdup_printf(
 "/cpus/cpu-map/socket%d/cluster%d/core%d/thread%d",
 cpu / (ms->smp.clusters * ms->smp.cores * ms->smp.threads),
 (cpu / (ms->smp.cores * ms->smp.threads)) % ms->smp.clusters,
 (cpu / ms->smp.threads) % ms->smp.cores,
 cpu % ms->smp.threads);
 } else {
 map_path = g_strdup_printf(
 "/cpus/cpu-map/socket%d/cluster%d/core%d",
 cpu / (ms->smp.clusters * ms->smp.cores),
 (cpu / ms->smp.cores) % ms->smp.clusters,
 cpu % ms->smp.cores);
 }
 qemu_fdt_add_path(ms->fdt, map_path);
 qemu_fdt_setprop_phandle(ms->fdt, map_path, "cpu", cpu_path);

 g_free(map_path);
 g_free(cpu_path);
 }
 }
}

static void fdt_add_its_gic_node(VirtMachineState *vms)
{
 char *nodename;
 MachineState *ms = MACHINE(vms);

 vms->msi_phandle = qemu_fdt_alloc_phandle(ms->fdt);
 nodename = g_strdup_printf("/intc/its@%" PRIx64,
 vms->memmap[VIRT_GIC_ITS].base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
 "arm,gic-v3-its");
 qemu_fdt_setprop(ms->fdt, nodename, "msi-controller", NULL, 0);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#msi-cells", 1);
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_ITS].base,
 2, vms->memmap[VIRT_GIC_ITS].size);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", vms->msi_phandle);
 g_free(nodename);
}

static void fdt_add_v2m_gic_node(VirtMachineState *vms)
{
 MachineState *ms = MACHINE(vms);
 char *nodename;

 nodename = g_strdup_printf("/intc/v2m@%" PRIx64,
 vms->memmap[VIRT_GIC_V2M].base);
 vms->msi_phandle = qemu_fdt_alloc_phandle(ms->fdt);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
 "arm,gic-v2m-frame");
 qemu_fdt_setprop(ms->fdt, nodename, "msi-controller", NULL, 0);
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_V2M].base,
 2, vms->memmap[VIRT_GIC_V2M].size);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", vms->msi_phandle);
 g_free(nodename);
}

static void fdt_add_gic_node(VirtMachineState *vms)
{
 MachineState *ms = MACHINE(vms);
 char *nodename;

 vms->gic_phandle = qemu_fdt_alloc_phandle(ms->fdt);
 qemu_fdt_setprop_cell(ms->fdt, "/", "interrupt-parent", vms->gic_phandle);

 nodename = g_strdup_printf("/intc@%" PRIx64,
 vms->memmap[VIRT_GIC_DIST].base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#interrupt-cells", 3);
 qemu_fdt_setprop(ms->fdt, nodename, "interrupt-controller", NULL, 0);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#address-cells", 0x2);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#size-cells", 0x2);
 qemu_fdt_setprop(ms->fdt, nodename, "ranges", NULL, 0);
 if (vms->gic_version != VIRT_GIC_VERSION_2) {
 int nb_redist_regions = virt_gicv3_redist_region_count(vms);

 qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
 "arm,gic-v3");

 if (!gunyah_enabled()) {
 qemu_fdt_setprop_cell(ms->fdt, nodename,
 "#redistributor-regions", nb_redist_regions);
 }

 if (nb_redist_regions == 1) {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_DIST].base,
 2, vms->memmap[VIRT_GIC_DIST].size,
 2, vms->memmap[VIRT_GIC_REDIST].base,
 2, vms->memmap[VIRT_GIC_REDIST].size);
 } else {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_DIST].base,
 2, vms->memmap[VIRT_GIC_DIST].size,
 2, vms->memmap[VIRT_GIC_REDIST].base,
 2, vms->memmap[VIRT_GIC_REDIST].size,
 2, vms->memmap[VIRT_HIGH_GIC_REDIST2].base,
 2, vms->memmap[VIRT_HIGH_GIC_REDIST2].size);
 }

 if (vms->virt) {
 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_GIC_MAINT_IRQ),
 GIC_FDT_IRQ_FLAGS_LEVEL_HI);
 }
 } else {

 qemu_fdt_setprop_string(ms->fdt, nodename, "compatible",
 "arm,cortex-a15-gic");
 if (!vms->virt) {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_DIST].base,
 2, vms->memmap[VIRT_GIC_DIST].size,
 2, vms->memmap[VIRT_GIC_CPU].base,
 2, vms->memmap[VIRT_GIC_CPU].size);
 } else {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, vms->memmap[VIRT_GIC_DIST].base,
 2, vms->memmap[VIRT_GIC_DIST].size,
 2, vms->memmap[VIRT_GIC_CPU].base,
 2, vms->memmap[VIRT_GIC_CPU].size,
 2, vms->memmap[VIRT_GIC_HYP].base,
 2, vms->memmap[VIRT_GIC_HYP].size,
 2, vms->memmap[VIRT_GIC_VCPU].base,
 2, vms->memmap[VIRT_GIC_VCPU].size);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(ARCH_GIC_MAINT_IRQ),
 GIC_FDT_IRQ_FLAGS_LEVEL_HI);
 }
 }

 qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", vms->gic_phandle);
 g_free(nodename);
}

static void fdt_add_pmu_nodes(const VirtMachineState *vms)
{
 ARMCPU *armcpu = ARM_CPU(first_cpu);
 uint32_t irqflags = GIC_FDT_IRQ_FLAGS_LEVEL_HI;
 MachineState *ms = MACHINE(vms);

 if (!arm_feature(&armcpu->env, ARM_FEATURE_PMU)) {
 assert(!object_property_get_bool(OBJECT(armcpu), "pmu", NULL));
 return;
 }

 if (vms->gic_version == VIRT_GIC_VERSION_2) {
 irqflags = deposit32(irqflags, GIC_FDT_IRQ_PPI_CPU_START,
 GIC_FDT_IRQ_PPI_CPU_WIDTH,
 (1 << MACHINE(vms)->smp.cpus) - 1);
 }

 qemu_fdt_add_subnode(ms->fdt, "/pmu");
 if (arm_feature(&armcpu->env, ARM_FEATURE_V8)) {
 const char compat[] = "arm,armv8-pmuv3";
 qemu_fdt_setprop(ms->fdt, "/pmu", "compatible",
 compat, sizeof(compat));
 qemu_fdt_setprop_cells(ms->fdt, "/pmu", "interrupts",
 GIC_FDT_IRQ_TYPE_PPI,
 INTID_TO_PPI(VIRTUAL_PMU_IRQ), irqflags);
 }
}

static void create_its(VirtMachineState *vms)
{
 const char *itsclass = its_class_name();
 DeviceState *dev;

 if (!strcmp(itsclass, "arm-gicv3-its")) {
 if (!vms->tcg_its) {
 itsclass = NULL;
 }
 } else if (!strcmp(itsclass, "arm-its-gunyah")) {

 itsclass = NULL;
 }

 if (!itsclass) {

 return;
 }

 dev = qdev_new(itsclass);

 object_property_set_link(OBJECT(dev), "parent-gicv3", OBJECT(vms->gic),
 &error_abort);
 sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
 sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, vms->memmap[VIRT_GIC_ITS].base);

 fdt_add_its_gic_node(vms);
 vms->msi_controller = VIRT_MSI_CTRL_ITS;
}

static void create_v2m(VirtMachineState *vms)
{
 int i;
 int irq = vms->irqmap[VIRT_GIC_V2M];
 DeviceState *dev;

 dev = qdev_new("arm-gicv2m");
 qdev_prop_set_uint32(dev, "base-spi", irq);
 qdev_prop_set_uint32(dev, "num-spi", NUM_GICV2M_SPIS);
 sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
 sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, vms->memmap[VIRT_GIC_V2M].base);

 for (i = 0; i < NUM_GICV2M_SPIS; i++) {
 sysbus_connect_irq(SYS_BUS_DEVICE(dev), i,
 qdev_get_gpio_in(vms->gic, irq + i));
 }

 fdt_add_v2m_gic_node(vms);
 vms->msi_controller = VIRT_MSI_CTRL_GICV2M;
}

static bool gicv3_nmi_present(VirtMachineState *vms)
{
 ARMCPU *cpu = ARM_CPU(qemu_get_cpu(0));

 return tcg_enabled() && cpu_isar_feature(aa64_nmi, cpu) &&
 (vms->gic_version != VIRT_GIC_VERSION_2);
}

static void create_gic(VirtMachineState *vms, MemoryRegion *mem)
{
 MachineState *ms = MACHINE(vms);

 SysBusDevice *gicbusdev;
 const char *gictype;
 int i;
 unsigned int smp_cpus = ms->smp.cpus;
 uint32_t nb_redist_regions = 0;
 int revision;

 if (vms->gic_version == VIRT_GIC_VERSION_2) {
 gictype = gic_class_name();
 } else {
 gictype = gicv3_class_name();
 }

 switch (vms->gic_version) {
 case VIRT_GIC_VERSION_2:
 revision = 2;
 break;
 case VIRT_GIC_VERSION_3:
 revision = 3;
 break;
 case VIRT_GIC_VERSION_4:
 revision = 4;
 break;
 default:
 g_assert_not_reached();
 }
 vms->gic = qdev_new(gictype);
 qdev_prop_set_uint32(vms->gic, "revision", revision);
 qdev_prop_set_uint32(vms->gic, "num-cpu", smp_cpus);

 qdev_prop_set_uint32(vms->gic, "num-irq", NUM_IRQS + 32);
 if (!false) {
 qdev_prop_set_bit(vms->gic, "has-security-extensions", vms->secure);
 }

 if (vms->gic_version != VIRT_GIC_VERSION_2) {
 QList *redist_region_count;
 uint32_t redist0_capacity = virt_redist_capacity(vms, VIRT_GIC_REDIST);
 uint32_t redist0_count = MIN(smp_cpus, redist0_capacity);

 nb_redist_regions = virt_gicv3_redist_region_count(vms);

 redist_region_count = qlist_new();
 qlist_append_int(redist_region_count, redist0_count);
 if (nb_redist_regions == 2) {
 uint32_t redist1_capacity =
 virt_redist_capacity(vms, VIRT_HIGH_GIC_REDIST2);

 qlist_append_int(redist_region_count,
 MIN(smp_cpus - redist0_count, redist1_capacity));
 }
 qdev_prop_set_array(vms->gic, "redist-region-count",
 redist_region_count);

 if (!false) {
 if (vms->tcg_its) {
 object_property_set_link(OBJECT(vms->gic), "sysmem",
 OBJECT(mem), &error_fatal);
 qdev_prop_set_bit(vms->gic, "has-lpi", true);
 }
 }
 } else {
 if (!false) {
 qdev_prop_set_bit(vms->gic, "has-virtualization-extensions",
 vms->virt);
 }
 }

 if (gicv3_nmi_present(vms)) {
 qdev_prop_set_bit(vms->gic, "has-nmi", true);
 }

 gicbusdev = SYS_BUS_DEVICE(vms->gic);
 sysbus_realize_and_unref(gicbusdev, &error_fatal);
 sysbus_mmio_map(gicbusdev, 0, vms->memmap[VIRT_GIC_DIST].base);
 if (vms->gic_version != VIRT_GIC_VERSION_2) {
 sysbus_mmio_map(gicbusdev, 1, vms->memmap[VIRT_GIC_REDIST].base);
 if (nb_redist_regions == 2) {
 sysbus_mmio_map(gicbusdev, 2,
 vms->memmap[VIRT_HIGH_GIC_REDIST2].base);
 }
 } else {
 sysbus_mmio_map(gicbusdev, 1, vms->memmap[VIRT_GIC_CPU].base);
 if (vms->virt) {
 sysbus_mmio_map(gicbusdev, 2, vms->memmap[VIRT_GIC_HYP].base);
 sysbus_mmio_map(gicbusdev, 3, vms->memmap[VIRT_GIC_VCPU].base);
 }
 }

 for (i = 0; i < smp_cpus; i++) {
 DeviceState *cpudev = DEVICE(qemu_get_cpu(i));
 int intidbase = NUM_IRQS + i * GIC_INTERNAL;

 const int timer_irq[] = {
 [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
 [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
 [GTIMER_HYP] = ARCH_TIMER_NS_EL2_IRQ,
 [GTIMER_SEC] = ARCH_TIMER_S_EL1_IRQ,
 [GTIMER_HYPVIRT] = ARCH_TIMER_NS_EL2_VIRT_IRQ,
 [GTIMER_S_EL2_PHYS] = ARCH_TIMER_S_EL2_IRQ,
 [GTIMER_S_EL2_VIRT] = ARCH_TIMER_S_EL2_VIRT_IRQ,
 };

 for (unsigned irq = 0; irq < ARRAY_SIZE(timer_irq); irq++) {
 qdev_connect_gpio_out(cpudev, irq,
 qdev_get_gpio_in(vms->gic,
 intidbase + timer_irq[irq]));
 }

 if (vms->gic_version != VIRT_GIC_VERSION_2) {
 qemu_irq irq = qdev_get_gpio_in(vms->gic,
 intidbase + ARCH_GIC_MAINT_IRQ);
 qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt",
 0, irq);
 } else if (vms->virt) {
 qemu_irq irq = qdev_get_gpio_in(vms->gic,
 intidbase + ARCH_GIC_MAINT_IRQ);
 sysbus_connect_irq(gicbusdev, i + 4 * smp_cpus, irq);
 }

 qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0,
 qdev_get_gpio_in(vms->gic, intidbase
 + VIRTUAL_PMU_IRQ));

 sysbus_connect_irq(gicbusdev, i, qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
 sysbus_connect_irq(gicbusdev, i + smp_cpus,
 qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
 sysbus_connect_irq(gicbusdev, i + 2 * smp_cpus,
 qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
 sysbus_connect_irq(gicbusdev, i + 3 * smp_cpus,
 qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));

 if (vms->gic_version != VIRT_GIC_VERSION_2) {
 sysbus_connect_irq(gicbusdev, i + 4 * smp_cpus,
 qdev_get_gpio_in(cpudev, ARM_CPU_NMI));
 sysbus_connect_irq(gicbusdev, i + 5 * smp_cpus,
 qdev_get_gpio_in(cpudev, ARM_CPU_VINMI));
 }
 }

 fdt_add_gic_node(vms);

 if (vms->gic_version != VIRT_GIC_VERSION_2 && vms->its) {
 create_its(vms);
 } else if (vms->gic_version == VIRT_GIC_VERSION_2) {
 create_v2m(vms);
 }
}

static void create_uart(const VirtMachineState *vms, int uart,
 MemoryRegion *mem, Chardev *chr, bool secure)
{
 char *nodename;
 hwaddr base = vms->memmap[uart].base;
 hwaddr size = vms->memmap[uart].size;
 int irq = vms->irqmap[uart];
 const char compat[] = "arm,pl011\0arm,primecell";
 const char clocknames[] = "uartclk\0apb_pclk";
 DeviceState *dev = qdev_new(TYPE_PL011);
 SysBusDevice *s = SYS_BUS_DEVICE(dev);
 MachineState *ms = MACHINE(vms);

 qdev_prop_set_chr(dev, "chardev", chr);
 sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
 memory_region_add_subregion(mem, base,
 sysbus_mmio_get_region(s, 0));
 sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));

 nodename = g_strdup_printf("/pl011@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);

 qemu_fdt_setprop(ms->fdt, nodename, "compatible",
 compat, sizeof(compat));
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, base, 2, size);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
 GIC_FDT_IRQ_TYPE_SPI, irq,
 GIC_FDT_IRQ_FLAGS_LEVEL_HI);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "clocks",
 vms->clock_phandle, vms->clock_phandle);
 qemu_fdt_setprop(ms->fdt, nodename, "clock-names",
 clocknames, sizeof(clocknames));

 if (uart == VIRT_UART0) {
 qemu_fdt_setprop_string(ms->fdt, "/chosen", "stdout-path", nodename);
 qemu_fdt_setprop_string(ms->fdt, "/aliases", "serial0", nodename);
 } else {
 qemu_fdt_setprop_string(ms->fdt, "/aliases", "serial1", nodename);
 }
 if (secure) {

 qemu_fdt_setprop_string(ms->fdt, nodename, "status", "disabled");
 qemu_fdt_setprop_string(ms->fdt, nodename, "secure-status", "okay");

 qemu_fdt_setprop_string(ms->fdt, "/secure-chosen", "stdout-path",
 nodename);
 }

 g_free(nodename);
}

static void create_rtc(const VirtMachineState *vms)
{
 char *nodename;
 hwaddr base = vms->memmap[VIRT_RTC].base;
 hwaddr size = vms->memmap[VIRT_RTC].size;
 int irq = vms->irqmap[VIRT_RTC];
 const char compat[] = "arm,pl031\0arm,primecell";
 MachineState *ms = MACHINE(vms);

 sysbus_create_simple("pl031", base, qdev_get_gpio_in(vms->gic, irq));

 nodename = g_strdup_printf("/pl031@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, base, 2, size);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
 GIC_FDT_IRQ_TYPE_SPI, irq,
 GIC_FDT_IRQ_FLAGS_LEVEL_HI);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "clocks", vms->clock_phandle);
 qemu_fdt_setprop_string(ms->fdt, nodename, "clock-names", "apb_pclk");
 g_free(nodename);
}

static DeviceState *gpio_key_dev;
static void virt_powerdown_req(Notifier *n, void *opaque)
{
 qemu_set_irq(qdev_get_gpio_in(gpio_key_dev, 0), 1);
}

static void create_gpio_keys(char *fdt, DeviceState *pl061_dev,
 uint32_t phandle)
{
 gpio_key_dev = sysbus_create_simple("gpio-key", -1,
 qdev_get_gpio_in(pl061_dev,
 GPIO_PIN_POWER_BUTTON));

 qemu_fdt_add_subnode(fdt, "/gpio-keys");
 qemu_fdt_setprop_string(fdt, "/gpio-keys", "compatible", "gpio-keys");

 qemu_fdt_add_subnode(fdt, "/gpio-keys/poweroff");
 qemu_fdt_setprop_string(fdt, "/gpio-keys/poweroff",
 "label", "GPIO Key Poweroff");
 qemu_fdt_setprop_cell(fdt, "/gpio-keys/poweroff", "linux,code",
 KEY_POWER);
 qemu_fdt_setprop_cells(fdt, "/gpio-keys/poweroff",
 "gpios", phandle, GPIO_PIN_POWER_BUTTON, 0);
}

#define SECURE_GPIO_POWEROFF 0
#define SECURE_GPIO_RESET 1

static void create_secure_gpio_pwr(char *fdt, DeviceState *pl061_dev,
 uint32_t phandle)
{
 DeviceState *gpio_pwr_dev;

 gpio_pwr_dev = sysbus_create_simple("gpio-pwr", -1, NULL);

 qdev_connect_gpio_out(pl061_dev, SECURE_GPIO_RESET,
 qdev_get_gpio_in_named(gpio_pwr_dev, "reset", 0));
 qdev_connect_gpio_out(pl061_dev, SECURE_GPIO_POWEROFF,
 qdev_get_gpio_in_named(gpio_pwr_dev, "shutdown", 0));

 qemu_fdt_add_subnode(fdt, "/gpio-poweroff");
 qemu_fdt_setprop_string(fdt, "/gpio-poweroff", "compatible",
 "gpio-poweroff");
 qemu_fdt_setprop_cells(fdt, "/gpio-poweroff",
 "gpios", phandle, SECURE_GPIO_POWEROFF, 0);
 qemu_fdt_setprop_string(fdt, "/gpio-poweroff", "status", "disabled");
 qemu_fdt_setprop_string(fdt, "/gpio-poweroff", "secure-status",
 "okay");

 qemu_fdt_add_subnode(fdt, "/gpio-restart");
 qemu_fdt_setprop_string(fdt, "/gpio-restart", "compatible",
 "gpio-restart");
 qemu_fdt_setprop_cells(fdt, "/gpio-restart",
 "gpios", phandle, SECURE_GPIO_RESET, 0);
 qemu_fdt_setprop_string(fdt, "/gpio-restart", "status", "disabled");
 qemu_fdt_setprop_string(fdt, "/gpio-restart", "secure-status",
 "okay");
}

static void create_gpio_devices(const VirtMachineState *vms, int gpio,
 MemoryRegion *mem)
{
 char *nodename;
 DeviceState *pl061_dev;
 hwaddr base = vms->memmap[gpio].base;
 hwaddr size = vms->memmap[gpio].size;
 int irq = vms->irqmap[gpio];
 const char compat[] = "arm,pl061\0arm,primecell";
 SysBusDevice *s;
 MachineState *ms = MACHINE(vms);

 pl061_dev = qdev_new("pl061");

 qdev_prop_set_uint32(pl061_dev, "pullups", 0);
 qdev_prop_set_uint32(pl061_dev, "pulldowns", 0xff);
 s = SYS_BUS_DEVICE(pl061_dev);
 sysbus_realize_and_unref(s, &error_fatal);
 memory_region_add_subregion(mem, base, sysbus_mmio_get_region(s, 0));
 sysbus_connect_irq(s, 0, qdev_get_gpio_in(vms->gic, irq));

 uint32_t phandle = qemu_fdt_alloc_phandle(ms->fdt);
 nodename = g_strdup_printf("/pl061@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, base, 2, size);
 qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#gpio-cells", 2);
 qemu_fdt_setprop(ms->fdt, nodename, "gpio-controller", NULL, 0);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
 GIC_FDT_IRQ_TYPE_SPI, irq,
 GIC_FDT_IRQ_FLAGS_LEVEL_HI);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "clocks", vms->clock_phandle);
 qemu_fdt_setprop_string(ms->fdt, nodename, "clock-names", "apb_pclk");
 qemu_fdt_setprop_cell(ms->fdt, nodename, "phandle", phandle);

 if (gpio != VIRT_GPIO) {

 qemu_fdt_setprop_string(ms->fdt, nodename, "status", "disabled");
 qemu_fdt_setprop_string(ms->fdt, nodename, "secure-status", "okay");
 }
 g_free(nodename);

 if (gpio == VIRT_GPIO) {
 create_gpio_keys(ms->fdt, pl061_dev, phandle);
 } else {
 create_secure_gpio_pwr(ms->fdt, pl061_dev, phandle);
 }
}

static bool virt_firmware_init(VirtMachineState *vms,
 MemoryRegion *sysmem,
 MemoryRegion *secure_sysmem)
{
 const char *bios_name;
 (void)sysmem;
 (void)secure_sysmem;

 bios_name = MACHINE(vms)->firmware;
 if (bios_name) {
 char *fname;
 int image_size;

 fname = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
 if (!fname) {
 error_report("Could not find ROM image '%s'", bios_name);
 exit(1);
 }

 hwaddr fw_base = vms->memmap[VIRT_MEM].base;
 image_size = load_image_targphys(fname, fw_base,
 vms->memmap[VIRT_MEM].size);
 if (image_size > 0) {
 gh_report("loaded firmware '%s' (%d bytes) "
 "at GPA 0x%"PRIx64, bios_name, image_size,
 (uint64_t)fw_base);
 }

 g_free(fname);
 if (image_size < 0) {
 error_report("Could not load ROM image '%s'", bios_name);
 exit(1);
 }
 }

 return bios_name != NULL;
}

static FWCfgState *create_fw_cfg(const VirtMachineState *vms, AddressSpace *as)
{
 MachineState *ms = MACHINE(vms);
 hwaddr base = vms->memmap[VIRT_FW_CFG].base;
 hwaddr size = vms->memmap[VIRT_FW_CFG].size;
 FWCfgState *fw_cfg;
 char *nodename;

 fw_cfg = fw_cfg_init_mem_wide(base + 8, base, 8, base + 16, as);
 fw_cfg_add_i16(fw_cfg, FW_CFG_NB_CPUS, (uint16_t)ms->smp.cpus);

 nodename = g_strdup_printf("/fw-cfg@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename,
 "compatible", "qemu,fw-cfg-mmio");
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, base, 2, size);
 qemu_fdt_setprop(ms->fdt, nodename, "dma-coherent", NULL, 0);
 g_free(nodename);
 return fw_cfg;
}

static void create_pcie_irq_map(const MachineState *ms,
 uint32_t gic_phandle,
 int first_irq, const char *nodename)
{
 int devfn, pin;
 uint32_t full_irq_map[4 * 4 * 10] = { 0 };
 uint32_t *irq_map = full_irq_map;

 for (devfn = 0; devfn <= 0x18; devfn += 0x8) {
 for (pin = 0; pin < 4; pin++) {
 int irq_type = GIC_FDT_IRQ_TYPE_SPI;
 int irq_nr = first_irq + ((pin + PCI_SLOT(devfn)) % PCI_NUM_PINS);
 int irq_level = GIC_FDT_IRQ_FLAGS_LEVEL_HI;
 int i;

 uint32_t map[] = {
 devfn << 8, 0, 0,
 pin + 1,
 gic_phandle, 0, 0, irq_type, irq_nr, irq_level };

 for (i = 0; i < 10; i++) {
 irq_map[i] = cpu_to_be32(map[i]);
 }
 irq_map += 10;
 }
 }

 qemu_fdt_setprop(ms->fdt, nodename, "interrupt-map",
 full_irq_map, sizeof(full_irq_map));

 qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupt-map-mask",
 cpu_to_be16(PCI_DEVFN(3, 0)),
 0, 0,
 0x7 );
}

static void create_virtio_iommu_dt_bindings(VirtMachineState *vms)
{
 const char compat[] = "virtio,pci-iommu\0pci1af4,1057";
 uint16_t bdf = vms->virtio_iommu_bdf;
 MachineState *ms = MACHINE(vms);
 char *node;

 vms->iommu_phandle = qemu_fdt_alloc_phandle(ms->fdt);

 node = g_strdup_printf("%s/virtio_iommu@%x,%x", vms->pciehb_nodename,
 PCI_SLOT(bdf), PCI_FUNC(bdf));
 qemu_fdt_add_subnode(ms->fdt, node);
 qemu_fdt_setprop(ms->fdt, node, "compatible", compat, sizeof(compat));
 qemu_fdt_setprop_sized_cells(ms->fdt, node, "reg",
 1, bdf << 8, 1, 0, 1, 0,
 1, 0, 1, 0);

 qemu_fdt_setprop_cell(ms->fdt, node, "#iommu-cells", 1);
 qemu_fdt_setprop_cell(ms->fdt, node, "phandle", vms->iommu_phandle);
 g_free(node);

 qemu_fdt_setprop_cells(ms->fdt, vms->pciehb_nodename, "iommu-map",
 0x0, vms->iommu_phandle, 0x0, bdf,
 bdf + 1, vms->iommu_phandle, bdf + 1, 0xffff - bdf);
}

static void create_pcie(VirtMachineState *vms)
{
 hwaddr base_mmio = vms->memmap[VIRT_PCIE_MMIO].base;
 hwaddr size_mmio = vms->memmap[VIRT_PCIE_MMIO].size;
 hwaddr base_mmio_high = vms->memmap[VIRT_HIGH_PCIE_MMIO].base;
 hwaddr size_mmio_high = vms->memmap[VIRT_HIGH_PCIE_MMIO].size;
 hwaddr base_pio = vms->memmap[VIRT_PCIE_PIO].base;
 hwaddr size_pio = vms->memmap[VIRT_PCIE_PIO].size;
 hwaddr base_ecam, size_ecam;
 hwaddr base = base_mmio;
 int nr_pcie_buses;
 int irq = vms->irqmap[VIRT_PCIE];
 MemoryRegion *mmio_alias;
 MemoryRegion *mmio_reg;
 MemoryRegion *ecam_alias;
 MemoryRegion *ecam_reg;
 DeviceState *dev;
 char *nodename;
 int i, ecam_id;
 PCIHostState *pci;
 MachineState *ms = MACHINE(vms);
 MachineClass *mc = MACHINE_GET_CLASS(ms);

 dev = qdev_new(TYPE_GPEX_HOST);
 sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

 ecam_id = VIRT_ECAM_ID(vms->highmem_ecam);
 base_ecam = vms->memmap[ecam_id].base;
 size_ecam = vms->memmap[ecam_id].size;
 nr_pcie_buses = size_ecam / PCIE_MMCFG_SIZE_MIN;

 ecam_alias = g_new0(MemoryRegion, 1);
 ecam_reg = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
 memory_region_init_alias(ecam_alias, OBJECT(dev), "pcie-ecam",
 ecam_reg, 0, size_ecam);
 memory_region_add_subregion(get_system_memory(), base_ecam, ecam_alias);

 mmio_alias = g_new0(MemoryRegion, 1);
 mmio_reg = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 1);
 memory_region_init_alias(mmio_alias, OBJECT(dev), "pcie-mmio",
 mmio_reg, base_mmio, size_mmio);
 memory_region_add_subregion(get_system_memory(), base_mmio, mmio_alias);

 if (vms->highmem_mmio) {

 MemoryRegion *high_mmio_alias = g_new0(MemoryRegion, 1);

 memory_region_init_alias(high_mmio_alias, OBJECT(dev), "pcie-mmio-high",
 mmio_reg, base_mmio_high, size_mmio_high);
 memory_region_add_subregion(get_system_memory(), base_mmio_high,
 high_mmio_alias);
 }

 sysbus_mmio_map(SYS_BUS_DEVICE(dev), 2, base_pio);

 for (i = 0; i < PCI_NUM_PINS; i++) {
 sysbus_connect_irq(SYS_BUS_DEVICE(dev), i,
 qdev_get_gpio_in(vms->gic, irq + i));
 gpex_set_irq_num(GPEX_HOST(dev), i, irq + i);
 }

 pci = PCI_HOST_BRIDGE(dev);
 pci->bypass_iommu = vms->default_bus_bypass_iommu;
 vms->bus = pci->bus;
 if (vms->bus) {
 pci_init_nic_devices(pci->bus, mc->default_nic);
 }

 nodename = vms->pciehb_nodename = g_strdup_printf("/pcie@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename,
 "compatible", "pci-host-ecam-generic");
 qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "pci");
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#address-cells", 3);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#size-cells", 2);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "linux,pci-domain", 0);
 qemu_fdt_setprop_cells(ms->fdt, nodename, "bus-range", 0,
 nr_pcie_buses - 1);
 qemu_fdt_setprop(ms->fdt, nodename, "dma-coherent", NULL, 0);

 if (vms->msi_phandle) {
 qemu_fdt_setprop_cells(ms->fdt, nodename, "msi-map",
 0, vms->msi_phandle, 0, 0x10000);
 }

 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, base_ecam, 2, size_ecam);

 if (vms->highmem_mmio) {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "ranges",
 1, FDT_PCI_RANGE_IOPORT, 2, 0,
 2, base_pio, 2, size_pio,
 1, FDT_PCI_RANGE_MMIO, 2, base_mmio,
 2, base_mmio, 2, size_mmio,
 1, FDT_PCI_RANGE_MMIO_64BIT,
 2, base_mmio_high,
 2, base_mmio_high, 2, size_mmio_high);
 } else {
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "ranges",
 1, FDT_PCI_RANGE_IOPORT, 2, 0,
 2, base_pio, 2, size_pio,
 1, FDT_PCI_RANGE_MMIO, 2, base_mmio,
 2, base_mmio, 2, size_mmio);
 }

 qemu_fdt_setprop_cell(ms->fdt, nodename, "#interrupt-cells", 1);
 create_pcie_irq_map(ms, vms->gic_phandle, irq, nodename);

}

static void create_platform_bus(VirtMachineState *vms)
{
 DeviceState *dev;
 SysBusDevice *s;
 int i;
 MemoryRegion *sysmem = get_system_memory();

 dev = qdev_new(TYPE_PLATFORM_BUS_DEVICE);
 dev->id = g_strdup(TYPE_PLATFORM_BUS_DEVICE);
 qdev_prop_set_uint32(dev, "num_irqs", PLATFORM_BUS_NUM_IRQS);
 qdev_prop_set_uint32(dev, "mmio_size", vms->memmap[VIRT_PLATFORM_BUS].size);
 sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
 vms->platform_bus_dev = dev;

 s = SYS_BUS_DEVICE(dev);
 for (i = 0; i < PLATFORM_BUS_NUM_IRQS; i++) {
 int irq = vms->irqmap[VIRT_PLATFORM_BUS] + i;
 sysbus_connect_irq(s, i, qdev_get_gpio_in(vms->gic, irq));
 }

 memory_region_add_subregion(sysmem,
 vms->memmap[VIRT_PLATFORM_BUS].base,
 sysbus_mmio_get_region(s, 0));
}

static void create_tag_ram(MemoryRegion *tag_sysmem,
 hwaddr base, hwaddr size,
 const char *name)
{
 MemoryRegion *tagram = g_new(MemoryRegion, 1);

 memory_region_init_ram(tagram, NULL, name, size / 32, &error_fatal);
 memory_region_add_subregion(tag_sysmem, base / 32, tagram);
}

static void create_secure_ram(VirtMachineState *vms,
 MemoryRegion *secure_sysmem,
 MemoryRegion *secure_tag_sysmem)
{
 MemoryRegion *secram = g_new(MemoryRegion, 1);
 char *nodename;
 hwaddr base = vms->memmap[VIRT_SECURE_MEM].base;
 hwaddr size = vms->memmap[VIRT_SECURE_MEM].size;
 MachineState *ms = MACHINE(vms);

 memory_region_init_ram(secram, NULL, "virt.secure-ram", size,
 &error_fatal);
 memory_region_add_subregion(secure_sysmem, base, secram);

 nodename = g_strdup_printf("/secram@%" PRIx64, base);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_string(ms->fdt, nodename, "device_type", "memory");
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg", 2, base, 2, size);
 qemu_fdt_setprop_string(ms->fdt, nodename, "status", "disabled");
 qemu_fdt_setprop_string(ms->fdt, nodename, "secure-status", "okay");

 if (secure_tag_sysmem) {
 create_tag_ram(secure_tag_sysmem, base, size, "mach-virt.secure-tag");
 }

 g_free(nodename);
}

static void *machvirt_dtb(const struct arm_boot_info *binfo, int *fdt_size)
{
 const VirtMachineState *board = container_of(binfo, VirtMachineState,
 bootinfo);
 MachineState *ms = MACHINE(board);

 *fdt_size = board->fdt_size;
 return ms->fdt;
}

static
void virt_machine_done(Notifier *notifier, void *data)
{
 VirtMachineState *vms = container_of(notifier, VirtMachineState,
 machine_done);
 MachineState *ms = MACHINE(vms);
 ARMCPU *cpu = ARM_CPU(first_cpu);
 struct arm_boot_info *info = &vms->bootinfo;
 AddressSpace *as = arm_boot_address_space(cpu, info);

 if (info->dtb_filename == NULL) {
 platform_bus_add_all_fdt_nodes(ms->fdt, "/intc",
 vms->memmap[VIRT_PLATFORM_BUS].base,
 vms->memmap[VIRT_PLATFORM_BUS].size,
 vms->irqmap[VIRT_PLATFORM_BUS]);
 }

 if (gunyah_enabled()) {

 GUNYAHState *gs = GUNYAH_STATE(current_accel());
 uint64_t gunyah_dtb_size = 0x200000;
 uint64_t main_mem_size = ms->ram_size;
 if (gs->protected_vm && gs->swiotlb_size) {
 main_mem_size -= gs->swiotlb_size;
 }
 if (info->firmware_loaded) {

 info->dtb_start = info->loader_start + 0x400000;
 } else {

 uint64_t mem_end = info->loader_start + main_mem_size;
 info->dtb_start = QEMU_ALIGN_DOWN(mem_end - gunyah_dtb_size,
 gunyah_dtb_size);
 }
 info->dtb_limit = 0;
 gh_report("DTB placed at dtb_start=0x%"PRIx64
 " dtb_size=0x%"PRIx64
 " main_mem=0x%"PRIx64" swiotlb=0x%"PRIx64
 " firmware=%d",
 (uint64_t)info->dtb_start, gunyah_dtb_size,
 main_mem_size, gs->swiotlb_size,
 info->firmware_loaded);
 }

 if (arm_load_dtb(info->dtb_start, info, info->dtb_limit, as, ms, cpu) < 0) {
 exit(1);
 }

 if (gunyah_enabled()) {

 uint64_t gunyah_dtb_size = 0x200000;
 if (gunyah_arm_set_dtb(info->dtb_start, gunyah_dtb_size)) {
 exit(1);
 }
 }

 pci_bus_add_fw_cfg_extra_pci_roots(vms->fw_cfg, vms->bus,
 &error_abort);

}

static uint64_t virt_cpu_mp_affinity(VirtMachineState *vms, int idx)
{
 uint8_t clustersz = ARM_DEFAULT_CPUS_PER_CLUSTER;
 VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);

 if (!vmc->disallow_affinity_adjustment) {

 if (vms->gic_version == VIRT_GIC_VERSION_2) {
 clustersz = GIC_TARGETLIST_BITS;
 } else {
 clustersz = GICV3_TARGETLIST_BITS;
 }
 }
 return arm_build_mp_affinity(idx, clustersz);
}

static inline bool *virt_get_high_memmap_enabled(VirtMachineState *vms,
 int index)
{
 bool *enabled_array[] = {
 &vms->highmem_redists,
 &vms->highmem_ecam,
 &vms->highmem_mmio,
 };

 assert(ARRAY_SIZE(extended_memmap) - VIRT_LOWMEMMAP_LAST ==
 ARRAY_SIZE(enabled_array));
 assert(index - VIRT_LOWMEMMAP_LAST < ARRAY_SIZE(enabled_array));

 return enabled_array[index - VIRT_LOWMEMMAP_LAST];
}

static void virt_set_high_memmap(VirtMachineState *vms,
 hwaddr base, int pa_bits)
{
 hwaddr region_base, region_size;
 bool *region_enabled, fits;
 int i;

 for (i = VIRT_LOWMEMMAP_LAST; i < ARRAY_SIZE(extended_memmap); i++) {
 region_enabled = virt_get_high_memmap_enabled(vms, i);
 region_base = ROUND_UP(base, extended_memmap[i].size);
 region_size = extended_memmap[i].size;

 vms->memmap[i].base = region_base;
 vms->memmap[i].size = region_size;

 fits = (region_base + region_size) <= BIT_ULL(pa_bits);
 *region_enabled &= fits;
 if (vms->highmem_compact && !*region_enabled) {
 continue;
 }

 base = region_base + region_size;
 if (fits) {
 vms->highest_gpa = base - 1;
 }
 }
}

static void virt_set_memmap(VirtMachineState *vms, int pa_bits)
{
 MachineState *ms = MACHINE(vms);
 hwaddr base, device_memory_base, device_memory_size, memtop;
 int i;

 vms->memmap = extended_memmap;

 for (i = 0; i < ARRAY_SIZE(base_memmap); i++) {
 vms->memmap[i] = base_memmap[i];
 }

 if (gunyah_enabled()) {
 vms->memmap[VIRT_MEM].base = 2 * GiB;
 }

 if (!vms->highmem) {
 pa_bits = 32;
 }

 device_memory_base =
 ROUND_UP(vms->memmap[VIRT_MEM].base + ms->ram_size, GiB);
 device_memory_size = ms->maxram_size - ms->ram_size + ms->ram_slots * GiB;

 memtop = base = device_memory_base + ROUND_UP(device_memory_size, GiB);
 if (memtop > BIT_ULL(pa_bits)) {
 error_report("Addressing limited to %d bits, but memory exceeds it by %llu bytes",
 pa_bits, memtop - BIT_ULL(pa_bits));
 exit(EXIT_FAILURE);
 }
 if (base < device_memory_base) {
 error_report("maxmem/slots too huge");
 exit(EXIT_FAILURE);
 }
 if (base < vms->memmap[VIRT_MEM].base + LEGACY_RAMLIMIT_BYTES) {
 base = vms->memmap[VIRT_MEM].base + LEGACY_RAMLIMIT_BYTES;
 }

 vms->highest_gpa = memtop - 1;

 virt_set_high_memmap(vms, base, pa_bits);

}

static VirtGICType finalize_gic_version_do(const char *accel_name,
 VirtGICType gic_version,
 int gics_supported,
 unsigned int max_cpus)
{

 switch (gic_version) {
 case VIRT_GIC_VERSION_HOST:
 if (!false) {
 error_report("gic-version=host requires KVM");
 exit(1);
 }

 return finalize_gic_version_do(accel_name, VIRT_GIC_VERSION_MAX,
 gics_supported, max_cpus);
 case VIRT_GIC_VERSION_MAX:
 if (gics_supported & VIRT_GIC_VERSION_4_MASK) {
 gic_version = VIRT_GIC_VERSION_4;
 } else if (gics_supported & VIRT_GIC_VERSION_3_MASK) {
 gic_version = VIRT_GIC_VERSION_3;
 } else {
 gic_version = VIRT_GIC_VERSION_2;
 }
 break;
 case VIRT_GIC_VERSION_NOSEL:
 if ((gics_supported & VIRT_GIC_VERSION_2_MASK) &&
 max_cpus <= GIC_NCPU) {
 gic_version = VIRT_GIC_VERSION_2;
 } else if (gics_supported & VIRT_GIC_VERSION_3_MASK) {

 gic_version = VIRT_GIC_VERSION_3;
 } else if (max_cpus > GIC_NCPU) {
 error_report("%s only supports GICv2 emulation but more than 8 "
 "vcpus are requested", accel_name);
 exit(1);
 }
 break;
 case VIRT_GIC_VERSION_2:
 case VIRT_GIC_VERSION_3:
 case VIRT_GIC_VERSION_4:
 break;
 }

 switch (gic_version) {
 case VIRT_GIC_VERSION_2:
 if (!(gics_supported & VIRT_GIC_VERSION_2_MASK)) {
 error_report("%s does not support GICv2 emulation", accel_name);
 exit(1);
 }
 break;
 case VIRT_GIC_VERSION_3:
 if (!(gics_supported & VIRT_GIC_VERSION_3_MASK)) {
 error_report("%s does not support GICv3 emulation", accel_name);
 exit(1);
 }
 break;
 case VIRT_GIC_VERSION_4:
 if (!(gics_supported & VIRT_GIC_VERSION_4_MASK)) {
 error_report("%s does not support GICv4 emulation, is virtualization=on?",
 accel_name);
 exit(1);
 }
 break;
 default:
 error_report("logic error in finalize_gic_version");
 exit(1);
 break;
 }

 return gic_version;
}

static void finalize_gic_version(VirtMachineState *vms)
{
 const char *accel_name = current_accel_name();
 unsigned int max_cpus = MACHINE(vms)->smp.max_cpus;
 int gics_supported = 0;

 if (gunyah_enabled()) {

 gics_supported |= VIRT_GIC_VERSION_3_MASK;
 } else if (tcg_enabled()) {
 gics_supported |= VIRT_GIC_VERSION_2_MASK;
 if (module_object_class_by_name("arm-gicv3")) {
 gics_supported |= VIRT_GIC_VERSION_3_MASK;
 if (vms->virt) {

 gics_supported |= VIRT_GIC_VERSION_4_MASK;
 }
 }
 } else {
 error_report("Unsupported accelerator, can not determine GIC support");
 exit(1);
 }

 vms->gic_version = finalize_gic_version_do(accel_name, vms->gic_version,
 gics_supported, max_cpus);
}

static void virt_cpu_post_init(VirtMachineState *vms, MemoryRegion *sysmem)
{
 bool aarch64, pmu;
 CPUState *cpu;

 aarch64 = object_property_get_bool(OBJECT(first_cpu), "aarch64", NULL);
 pmu = object_property_get_bool(OBJECT(first_cpu), "pmu", NULL);

 CPU_FOREACH(cpu) {
 if (pmu) {
 assert(arm_feature(&ARM_CPU(cpu)->env, ARM_FEATURE_PMU));
 }
 }

 if (aarch64 && vms->highmem) {
 int requested_pa_size = 64 - clz64(vms->highest_gpa);
 int pamax = arm_pamax(ARM_CPU(first_cpu));

 if (pamax < requested_pa_size) {
 error_report("VCPU supports less PA bits (%d) than "
 "requested by the memory map (%d)",
 pamax, requested_pa_size);
 exit(1);
 }
 }
}

#define TYPE_ARM_CONFIDENTIAL_GUEST "arm-confidential-guest"
OBJECT_DECLARE_SIMPLE_TYPE(ArmConfidentialGuestState, ARM_CONFIDENTIAL_GUEST)

struct ArmConfidentialGuestState {
 ConfidentialGuestSupport parent_obj;
 hwaddr swiotlb_size;
};

static void
arm_confidential_guest_get_swiotlb_size(Object *obj, Visitor *v,
 const char *name, void *opaque,
 Error **errp)
{
 ArmConfidentialGuestState *acg = ARM_CONFIDENTIAL_GUEST(obj);
 uint64_t value = acg->swiotlb_size;

 visit_type_size(v, name, &value, errp);
}

static void
arm_confidential_guest_set_swiotlb_size(Object *obj, Visitor *v,
 const char *name, void *opaque,
 Error **errp)
{
 ArmConfidentialGuestState *acg = ARM_CONFIDENTIAL_GUEST(obj);
 uint64_t value;

 if (!visit_type_size(v, name, &value, errp)) {
 return;
 }

 acg->swiotlb_size = value;
}

static void
arm_confidential_guest_instance_init(Object *obj)
{
 object_property_add(obj, "swiotlb-size", "size",
 arm_confidential_guest_get_swiotlb_size,
 arm_confidential_guest_set_swiotlb_size,
 NULL, NULL);
}

static const TypeInfo confidential_guest_info = {
 .parent = TYPE_CONFIDENTIAL_GUEST_SUPPORT,
 .name = TYPE_ARM_CONFIDENTIAL_GUEST,
 .instance_size = sizeof(ArmConfidentialGuestState),
 .instance_init = arm_confidential_guest_instance_init,
 .interfaces = (InterfaceInfo[]) {
 { TYPE_USER_CREATABLE },
 { }
 }
};

static void
confidential_guest_register_types(void)
{
 type_register_static(&confidential_guest_info);
}
type_init(confidential_guest_register_types);

static int confidential_guest_init(MachineState *ms)
{
 ConfidentialGuestSupport *cgs = ms->cgs;
 ArmConfidentialGuestState *obj;

 if (!cgs) {
 return 0;
 }

 obj = (ArmConfidentialGuestState *)
 object_dynamic_cast(OBJECT(cgs), TYPE_ARM_CONFIDENTIAL_GUEST);

 if (!obj) {
 return 0;
 }

 if (!gunyah_enabled()) {
 error_report("arm-confidential-guest requires -accel gunyah");
 return -1;
 }

 if (obj->swiotlb_size > ms->ram_size) {
 error_report("swiotlb-size (0x%"PRIx64") exceeds RAM size (0x%"PRIx64")",
 (uint64_t)obj->swiotlb_size, (uint64_t)ms->ram_size);
 return -1;
 }

 {
 GUNYAHState *s = GUNYAH_STATE(current_accel());
 s->protected_vm = true;

 if (obj->swiotlb_size) {
 gunyah_set_swiotlb_size(obj->swiotlb_size);
 gh_report("confidential-guest-support: "
 "protected_vm=true swiotlb=0x%"PRIx64,
 (uint64_t)obj->swiotlb_size);
 } else {
 gh_report("confidential-guest-support: "
 "protected_vm=true (no swiotlb specified, "
 "using default)");
 }
 }

 cgs->ready = true;
 return 0;
}

static void fdt_add_reserved_memory(VirtMachineState *vms)
{
 MachineState *ms = MACHINE(vms);
 GUNYAHState *gs = GUNYAH_STATE(current_accel());
 hwaddr membase = vms->memmap[VIRT_MEM].base;
 hwaddr memsize = ms->ram_size;
 hwaddr resv_start;
 const char compat[] = "restricted-dma-pool";
 char *nodename;

 if (!gs->protected_vm || !gs->swiotlb_size) {
 return;
 }

 nodename = g_strdup_printf("/reserved-memory");
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#address-cells", 2);
 qemu_fdt_setprop_cell(ms->fdt, nodename, "#size-cells", 2);
 qemu_fdt_setprop(ms->fdt, nodename, "ranges", NULL, 0);
 g_free(nodename);

 resv_start = membase + memsize - gs->swiotlb_size;
 nodename = g_strdup_printf("/reserved-memory/restricted_dma_reserved@%"
 PRIx64, resv_start);
 qemu_fdt_add_subnode(ms->fdt, nodename);
 qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg",
 2, resv_start,
 2, gs->swiotlb_size);
 qemu_fdt_setprop(ms->fdt, nodename, "compatible", compat, sizeof(compat));
 g_free(nodename);
}

static void virt_modify_dtb(const struct arm_boot_info *binfo, void *fdt)
{
 const VirtMachineState *vms = container_of(binfo, VirtMachineState,
 bootinfo);
 MachineState *ms = MACHINE(vms);
 uint64_t mem_base = vms->memmap[VIRT_MEM].base;
 uint64_t mem_size = ms->ram_size;
 int ret;

 gh_report("Building minimal DTB from scratch (mem_base=0x%"PRIx64
 " mem_size=0x%"PRIx64")", mem_base, mem_size);

 ret = fdt_create_empty_tree(fdt, 0x100000 );
 if (ret) {
 gh_report("fdt_create_empty_tree failed: %s",
 fdt_strerror(ret));
 exit(1);
 }

 fdt_setprop_string(fdt, 0, "compatible", "linux,dummy-virt");
 fdt_setprop_string(fdt, 0, "model", "QEMU Gunyah Virtual Machine");
 fdt_setprop_cell(fdt, 0, "interrupt-parent", 1);
 fdt_setprop_cell(fdt, 0, "#address-cells", 2);
 fdt_setprop_cell(fdt, 0, "#size-cells", 2);

 {
 int chosen_off;
 char bootargs[1024];
 const char *user_cmdline = binfo->kernel_cmdline;
 const char *earlycon_args =
 "earlycon=pl011,mmio32,0x09000000 "
 "console=ttyAMA0";

 chosen_off = fdt_add_subnode(fdt, 0, "chosen");
 if (user_cmdline && *user_cmdline) {
 snprintf(bootargs, sizeof(bootargs),
 "%s %s", earlycon_args, user_cmdline);
 } else {
 snprintf(bootargs, sizeof(bootargs), "%s", earlycon_args);
 }
 fdt_setprop_string(fdt, chosen_off, "bootargs", bootargs);
 fdt_setprop_string(fdt, chosen_off, "stdout-path",
 "/pl011@9000000");
 gh_report("DTB /chosen/bootargs: %s", bootargs);
 gh_report("DTB /chosen/stdout-path: /pl011@9000000");
 }

 {
 int aliases_off = fdt_add_subnode(fdt, 0, "aliases");
 fdt_setprop_string(fdt, aliases_off, "serial0", "/pl011@9000000");
 }

 {
 int cfg_off;
 uint32_t cfg_addr, cfg_size;

 cfg_off = fdt_add_subnode(fdt, 0, "config");
 cfg_addr = (uint32_t)mem_base;
 cfg_size = 0x1000000;
 fdt_setprop_cell(fdt, cfg_off, "kernel-address", cfg_addr);
 fdt_setprop_cell(fdt, cfg_off, "kernel-size", cfg_size);
 gh_report("DTB /config: kernel-address=0x%x kernel-size=0x%x"
 "%s", cfg_addr, cfg_size,
 binfo->firmware_loaded ? " (firmware)" : "");
 }

 {
 int memoff;
 fdt64_t mem_reg[2];
 GUNYAHState *gs_mem = get_gunyah_state();
 uint64_t dtb_mem_size = mem_size;

 if (gs_mem->protected_vm && gs_mem->swiotlb_size) {
 dtb_mem_size = mem_size - gs_mem->swiotlb_size;
 }
 memoff = fdt_add_subnode(fdt, 0, "memory");
 fdt_setprop_string(fdt, memoff, "device_type", "memory");
 mem_reg[0] = cpu_to_fdt64(mem_base);
 mem_reg[1] = cpu_to_fdt64(dtb_mem_size);
 fdt_setprop(fdt, memoff, "reg", mem_reg, sizeof(mem_reg));
 gh_report("DTB /memory: base=0x%"PRIx64" size=0x%"PRIx64
 " (total=0x%"PRIx64" lend_only=%d)"
 "%s", mem_base, dtb_mem_size,
 mem_size,
 (gs_mem->protected_vm && gs_mem->swiotlb_size) ? 1 : 0,
 binfo->firmware_loaded ? " (firmware)" : "");
 }

 {
 int cpus_off, cpu_off, i;
 int num_cpus = ms->smp.cpus;
 char cpuname[32];

 cpus_off = fdt_add_subnode(fdt, 0, "cpus");
 fdt_setprop_cell(fdt, cpus_off, "#address-cells", 1);
 fdt_setprop_cell(fdt, cpus_off, "#size-cells", 0);

 for (i = 0; i < num_cpus; i++) {
 snprintf(cpuname, sizeof(cpuname), "cpu@%d", i);
 cpu_off = fdt_add_subnode(fdt, cpus_off, cpuname);
 fdt_setprop_string(fdt, cpu_off, "device_type", "cpu");
 fdt_setprop_string(fdt, cpu_off, "compatible", "arm,armv8");
 fdt_setprop_cell(fdt, cpu_off, "reg", i);
 fdt_setprop_cell(fdt, cpu_off, "phandle", 0x100 + i);
 if (num_cpus > 1) {
 fdt_setprop_string(fdt, cpu_off, "enable-method", "psci");
 }
 }
 gh_report("DTB /cpus: %d CPUs with%s PSCI",
 num_cpus, num_cpus > 1 ? "" : "out");
 }

 {
 int intc_off;
 uint32_t redist_size = 0x20000 * ms->smp.cpus;
 fdt64_t gic_reg[4];

 intc_off = fdt_add_subnode(fdt, 0, "intc");
 fdt_setprop_string(fdt, intc_off, "compatible", "arm,gic-v3");
 fdt_setprop_cell(fdt, intc_off, "#interrupt-cells", 3);
 fdt_setprop(fdt, intc_off, "interrupt-controller", NULL, 0);
 fdt_setprop_cell(fdt, intc_off, "#address-cells", 2);
 fdt_setprop_cell(fdt, intc_off, "#size-cells", 2);

 gic_reg[0] = cpu_to_fdt64(0x08000000);
 gic_reg[1] = cpu_to_fdt64(0x10000);
 gic_reg[2] = cpu_to_fdt64(0x080A0000);
 gic_reg[3] = cpu_to_fdt64(redist_size);
 fdt_setprop(fdt, intc_off, "reg", gic_reg, sizeof(gic_reg));
 fdt_setprop_cell(fdt, intc_off, "phandle", 1);
 }

 {
 int timer_off;
 fdt32_t timer_irqs[12] = {
 cpu_to_fdt32(1), cpu_to_fdt32(13), cpu_to_fdt32(0x108),
 cpu_to_fdt32(1), cpu_to_fdt32(14), cpu_to_fdt32(0x108),
 cpu_to_fdt32(1), cpu_to_fdt32(11), cpu_to_fdt32(0x108),
 cpu_to_fdt32(1), cpu_to_fdt32(10), cpu_to_fdt32(0x108),
 };
 timer_off = fdt_add_subnode(fdt, 0, "timer");
 fdt_setprop_string(fdt, timer_off, "compatible", "arm,armv8-timer");
 fdt_setprop(fdt, timer_off, "interrupts", timer_irqs,
 sizeof(timer_irqs));
 fdt_setprop(fdt, timer_off, "always-on", NULL, 0);
 }

 {
 int psci_off;
 psci_off = fdt_add_subnode(fdt, 0, "psci");
 fdt_setprop_string(fdt, psci_off, "compatible", "arm,psci-0.2");
 fdt_setprop_string(fdt, psci_off, "method", "hvc");
 }

 {
 GUNYAHState *gs = get_gunyah_state();
 if (gs->protected_vm && gs->swiotlb_size) {
 int resv_off, pool_off;
 char poolname[64];
 uint64_t resv_start = mem_base + mem_size - gs->swiotlb_size;
 fdt64_t resv_reg[2];
 const char compat[] = "restricted-dma-pool";

 resv_off = fdt_add_subnode(fdt, 0, "reserved-memory");
 fdt_setprop_cell(fdt, resv_off, "#address-cells", 2);
 fdt_setprop_cell(fdt, resv_off, "#size-cells", 2);
 fdt_setprop(fdt, resv_off, "ranges", NULL, 0);

 snprintf(poolname, sizeof(poolname),
 "restricted_dma_reserved@%"PRIx64, resv_start);
 pool_off = fdt_add_subnode(fdt, resv_off, poolname);
 resv_reg[0] = cpu_to_fdt64(resv_start);
 resv_reg[1] = cpu_to_fdt64(gs->swiotlb_size);
 fdt_setprop(fdt, pool_off, "reg", resv_reg, sizeof(resv_reg));
 fdt_setprop(fdt, pool_off, "compatible", compat, sizeof(compat));
 {
 fdt64_t alignment = cpu_to_fdt64(0x1000);
 fdt_setprop(fdt, pool_off, "alignment", &alignment,
 sizeof(alignment));
 }
 fdt_setprop_cell(fdt, pool_off, "phandle", 2);

 gh_report("DTB reserved-memory: restricted-dma-pool at "
 "0x%"PRIx64" size 0x%"PRIx64,
 resv_start, gs->swiotlb_size);
 }
 }

 {
 int pci_off;
 uint64_t base_ecam = vms->memmap[VIRT_PCIE_ECAM].base;
 uint64_t size_ecam = vms->memmap[VIRT_PCIE_ECAM].size;
 uint64_t base_mmio_pci = vms->memmap[VIRT_PCIE_MMIO].base;
 uint64_t size_mmio_pci = vms->memmap[VIRT_PCIE_MMIO].size;
 uint64_t base_pio = vms->memmap[VIRT_PCIE_PIO].base;
 uint64_t size_pio = vms->memmap[VIRT_PCIE_PIO].size;
 int first_irq = 3;

 int nr_pcie_buses = 1;
 GUNYAHState *gs_pci = get_gunyah_state();
 char pci_node_name[32];
 snprintf(pci_node_name, sizeof(pci_node_name),
 "pcie@%"PRIx64, base_mmio_pci);

 pci_off = fdt_add_subnode(fdt, 0, pci_node_name);
 fdt_setprop_string(fdt, pci_off, "compatible",
 "pci-host-ecam-generic");
 fdt_setprop_string(fdt, pci_off, "device_type", "pci");
 fdt_setprop_cell(fdt, pci_off, "#address-cells", 3);
 fdt_setprop_cell(fdt, pci_off, "#size-cells", 2);
 fdt_setprop_cell(fdt, pci_off, "linux,pci-domain", 0);
 {
 fdt32_t bus_range[2] = {
 cpu_to_fdt32(0), cpu_to_fdt32(nr_pcie_buses - 1)
 };
 fdt_setprop(fdt, pci_off, "bus-range", bus_range,
 sizeof(bus_range));
 }
 fdt_setprop(fdt, pci_off, "dma-coherent", NULL, 0);

 {
 fdt64_t ecam_reg[2] = {
 cpu_to_fdt64(base_ecam), cpu_to_fdt64(size_ecam)
 };
 fdt_setprop(fdt, pci_off, "reg", ecam_reg, sizeof(ecam_reg));
 }

 {
 fdt32_t ranges[14];
 int idx = 0;

 ranges[idx++] = cpu_to_fdt32(0x01000000);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(base_pio);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(size_pio);

 ranges[idx++] = cpu_to_fdt32(0x02000000);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(base_mmio_pci);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(base_mmio_pci);
 ranges[idx++] = cpu_to_fdt32(0);
 ranges[idx++] = cpu_to_fdt32(size_mmio_pci);

 fdt_setprop(fdt, pci_off, "ranges", ranges,
 idx * sizeof(fdt32_t));
 }

 fdt_setprop_cell(fdt, pci_off, "#interrupt-cells", 1);
 {
 fdt32_t irq_map[16 * 10];
 int mi = 0, devfn, pin;

 for (devfn = 0; devfn <= 0x18; devfn += 0x8) {
 for (pin = 0; pin < 4; pin++) {
 int irq_nr = first_irq +
 ((pin + (devfn >> 3)) % 4);
 irq_map[mi++] = cpu_to_fdt32(devfn << 8);
 irq_map[mi++] = cpu_to_fdt32(0);
 irq_map[mi++] = cpu_to_fdt32(0);
 irq_map[mi++] = cpu_to_fdt32(pin + 1);
 irq_map[mi++] = cpu_to_fdt32(1);
 irq_map[mi++] = cpu_to_fdt32(0);
 irq_map[mi++] = cpu_to_fdt32(0);
 irq_map[mi++] = cpu_to_fdt32(0);
 irq_map[mi++] = cpu_to_fdt32(irq_nr);
 irq_map[mi++] = cpu_to_fdt32(4);
 }
 }
 fdt_setprop(fdt, pci_off, "interrupt-map", irq_map,
 mi * sizeof(fdt32_t));
 }
 {
 fdt32_t irq_mask[4] = {
 cpu_to_fdt32(0x1800), cpu_to_fdt32(0),
 cpu_to_fdt32(0), cpu_to_fdt32(7)
 };
 fdt_setprop(fdt, pci_off, "interrupt-map-mask", irq_mask,
 sizeof(irq_mask));
 }

 if (gs_pci->protected_vm && gs_pci->swiotlb_size) {
 fdt_setprop_cell(fdt, pci_off, "memory-region", 2);
 }

 gh_report("DTB PCI host bridge: ECAM=0x%"PRIx64
 " MMIO=0x%"PRIx64"-0x%"PRIx64
 " PIO=0x%"PRIx64" IRQs SPI %d-%d",
 base_ecam, base_mmio_pci,
 base_mmio_pci + size_mmio_pci - 1,
 base_pio, first_irq, first_irq + 3);
 }

 {
 int clk_off = fdt_add_subnode(fdt, 0, "apb-pclk");
 fdt_setprop_string(fdt, clk_off, "compatible", "fixed-clock");
 fdt_setprop_cell(fdt, clk_off, "#clock-cells", 0);
 fdt_setprop_cell(fdt, clk_off, "clock-frequency", 24000000);
 fdt_setprop_string(fdt, clk_off, "clock-output-names", "clk24mhz");
 fdt_setprop_cell(fdt, clk_off, "phandle", 3);
 gh_report("DTB /apb-pclk: 24MHz fixed clock (phandle=3)");
 }

 {
 int uart_off = fdt_add_subnode(fdt, 0, "pl011@9000000");
 const char compat[] = "arm,pl011\0arm,primecell";
 const char clocknames[] = "uartclk\0apb_pclk";
 fdt64_t uart_reg[2] = {
 cpu_to_fdt64(0x09000000), cpu_to_fdt64(0x1000)
 };
 fdt32_t uart_irq[3] = {
 cpu_to_fdt32(0),
 cpu_to_fdt32(1),
 cpu_to_fdt32(4)
 };
 fdt32_t uart_clocks[2] = {
 cpu_to_fdt32(3), cpu_to_fdt32(3)
 };

 fdt_setprop(fdt, uart_off, "compatible", compat, sizeof(compat));
 fdt_setprop(fdt, uart_off, "reg", uart_reg, sizeof(uart_reg));
 fdt_setprop(fdt, uart_off, "interrupts", uart_irq, sizeof(uart_irq));
 fdt_setprop(fdt, uart_off, "clocks", uart_clocks, sizeof(uart_clocks));
 fdt_setprop(fdt, uart_off, "clock-names",
 clocknames, sizeof(clocknames));

 fdt_setprop_cell(fdt, uart_off, "clock-frequency", 24000000);
 fdt_setprop(fdt, uart_off, "dma-coherent", NULL, 0);

 {
 GUNYAHState *gs_uart = get_gunyah_state();
 if (gs_uart && gs_uart->protected_vm && gs_uart->swiotlb_size) {
 fdt_setprop_cell(fdt, uart_off, "memory-region", 2);
 }
 }
 gh_report("DTB /pl011@9000000: ttyAMA0, SPI 1, level-high, 24MHz");
 }

 {
 int fwcfg_off = fdt_add_subnode(fdt, 0, "fw-cfg@9020000");

 fdt64_t fwcfg_reg[2] = {
 cpu_to_fdt64(0x09020000), cpu_to_fdt64(0x10)
 };
 fdt_setprop_string(fdt, fwcfg_off, "compatible", "qemu,fw-cfg-mmio");
 fdt_setprop(fdt, fwcfg_off, "reg", fwcfg_reg, sizeof(fwcfg_reg));
 fdt_setprop(fdt, fwcfg_off, "dma-coherent", NULL, 0);
 gh_report("DTB /fw-cfg@9020000: fw_cfg MMIO at 0x09020000"
 " (no DMA - MMIO-only for Gunyah safety)");
 }

 {
 GUNYAHState *gs_sfb = get_gunyah_state();
 if (gs_sfb->protected_vm && 0 ) {
 uint32_t sfb_width = 1280, sfb_height = 720;
 uint32_t sfb_stride = sfb_width * 4;
 uint64_t sfb_size = (uint64_t)sfb_stride * sfb_height;
 uint64_t sfb_addr;

 sfb_size = (sfb_size + 0x1fffff) & ~0x1fffffULL;

 if (gs_sfb->swiotlb_size > 0) {

 sfb_addr = mem_base + mem_size - gs_sfb->swiotlb_size;
 } else {

 sfb_addr = mem_base + mem_size - sfb_size;
 }

 {
 extern void simplefb_start(uint64_t, uint32_t, uint32_t);
 simplefb_start(sfb_addr, sfb_width, sfb_height);
 }

 {
 int rsvd_off, rsvd_fb_off;
 fdt64_t rsvd_reg[2];
 char rsvd_name[64];

 rsvd_off = fdt_add_subnode(fdt, 0, "reserved-memory");
 fdt_setprop_cell(fdt, rsvd_off, "#address-cells", 2);
 fdt_setprop_cell(fdt, rsvd_off, "#size-cells", 2);
 fdt_setprop(fdt, rsvd_off, "ranges", NULL, 0);

 snprintf(rsvd_name, sizeof(rsvd_name),
 "framebuffer@%"PRIx64, sfb_addr);
 rsvd_fb_off = fdt_add_subnode(fdt, rsvd_off, rsvd_name);
 rsvd_reg[0] = cpu_to_fdt64(sfb_addr);
 rsvd_reg[1] = cpu_to_fdt64(sfb_size);
 fdt_setprop(fdt, rsvd_fb_off, "reg",
 rsvd_reg, sizeof(rsvd_reg));
 fdt_setprop(fdt, rsvd_fb_off, "no-map", NULL, 0);

 gh_report("DTB /reserved-memory/framebuffer: "
 "0x%"PRIx64" size=0x%"PRIx64" (no-map)",
 sfb_addr, sfb_size);
 }

 {
 int sfb_off;
 char sfb_name[64];
 fdt64_t sfb_reg[2];

 snprintf(sfb_name, sizeof(sfb_name),
 "framebuffer@%"PRIx64, sfb_addr);
 sfb_off = fdt_add_subnode(fdt, 0, sfb_name);
 fdt_setprop_string(fdt, sfb_off, "compatible",
 "simple-framebuffer");
 sfb_reg[0] = cpu_to_fdt64(sfb_addr);
 sfb_reg[1] = cpu_to_fdt64(sfb_size);
 fdt_setprop(fdt, sfb_off, "reg", sfb_reg, sizeof(sfb_reg));
 fdt_setprop_cell(fdt, sfb_off, "width", sfb_width);
 fdt_setprop_cell(fdt, sfb_off, "height", sfb_height);
 fdt_setprop_cell(fdt, sfb_off, "stride", sfb_stride);
 fdt_setprop_string(fdt, sfb_off, "format", "a8r8g8b8");
 fdt_setprop_string(fdt, sfb_off, "status", "okay");

 gh_report("DTB /simplefb@0x%"PRIx64
 ": %ux%u stride=%u size=0x%"PRIx64,
 sfb_addr, sfb_width, sfb_height,
 sfb_stride, sfb_size);
 }
 }
 }

 gunyah_arm_fdt_customize(fdt, mem_base, 1 );

 {
 DeviceState *pbus_dev;
 pbus_dev = qdev_find_recursive(sysbus_get_default(),
 TYPE_PLATFORM_BUS_DEVICE);
 if (pbus_dev) {
 platform_bus_add_all_fdt_nodes(fdt, "/intc",
 vms->memmap[VIRT_PLATFORM_BUS].base,
 vms->memmap[VIRT_PLATFORM_BUS].size,
 vms->irqmap[VIRT_PLATFORM_BUS]);
 gh_report("DTB platform-bus at 0x%"PRIx64" (size 0x%"PRIx64
 ") with dynamic sysbus devices",
 (uint64_t)vms->memmap[VIRT_PLATFORM_BUS].base,
 (uint64_t)vms->memmap[VIRT_PLATFORM_BUS].size);
 }
 }

 {
 int sym_off;
 sym_off = fdt_add_subnode(fdt, 0, "__symbols__");
 fdt_setprop_string(fdt, sym_off, "intc", "/intc");
 }

 gh_report("Minimal DTB built with earlycon (totalsize=%u)",
 fdt_totalsize(fdt));
}

static void machvirt_init(MachineState *machine)
{
 VirtMachineState *vms = VIRT_MACHINE(machine);
 VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(machine);
 MachineClass *mc = MACHINE_GET_CLASS(machine);
 const CPUArchIdList *possible_cpus;
 MemoryRegion *sysmem = get_system_memory();
 MemoryRegion *secure_sysmem = NULL;
 MemoryRegion *tag_sysmem = NULL;
 MemoryRegion *secure_tag_sysmem = NULL;
 int n, virt_max_cpus;
 bool firmware_loaded;
 bool aarch64 = true;
 unsigned int smp_cpus = machine->smp.cpus;
 unsigned int max_cpus = machine->smp.max_cpus;

 possible_cpus = mc->possible_cpu_arch_ids(machine);

 if (confidential_guest_init(machine) != 0) {
 error_report("Failed to initialize confidential guest");
 exit(1);
 }

 if (gunyah_enabled()) {
 vms->highmem_ecam = false;
 vms->highmem_mmio = false;
 vms->highmem_redists = false;
 }

 if (!vms->memmap) {
 Object *cpuobj;
 ARMCPU *armcpu;
 int pa_bits;

 cpuobj = object_new(possible_cpus->cpus[0].type);
 armcpu = ARM_CPU(cpuobj);

 pa_bits = arm_pamax(armcpu);

 object_unref(cpuobj);

 virt_set_memmap(vms, pa_bits);
 }

 finalize_gic_version(vms);

 if (vms->secure) {

 secure_sysmem = g_new(MemoryRegion, 1);
 memory_region_init(secure_sysmem, OBJECT(machine), "secure-memory",
 UINT64_MAX);
 memory_region_add_subregion_overlap(secure_sysmem, 0, sysmem, -1);
 }

 firmware_loaded = virt_firmware_init(vms, sysmem,
 secure_sysmem ?: sysmem);

 if (vms->secure && firmware_loaded) {
 vms->psci_conduit = QEMU_PSCI_CONDUIT_DISABLED;
 } else if (vms->virt) {
 vms->psci_conduit = QEMU_PSCI_CONDUIT_SMC;
 } else {
 vms->psci_conduit = QEMU_PSCI_CONDUIT_HVC;
 }

 if (vms->gic_version == VIRT_GIC_VERSION_2) {
 virt_max_cpus = GIC_NCPU;
 } else {
 virt_max_cpus = virt_redist_capacity(vms, VIRT_GIC_REDIST);
 if (vms->highmem_redists) {
 virt_max_cpus += virt_redist_capacity(vms, VIRT_HIGH_GIC_REDIST2);
 }
 }

 if (max_cpus > virt_max_cpus) {
 error_report("Number of SMP CPUs requested (%d) exceeds max CPUs "
 "supported by machine 'mach-virt' (%d)",
 max_cpus, virt_max_cpus);
 if (vms->gic_version != VIRT_GIC_VERSION_2 && !vms->highmem_redists) {
 error_printf("Try 'highmem-redists=on' for more CPUs\n");
 }

 exit(1);
 }

 create_fdt(vms);

 assert(possible_cpus->len == max_cpus);
 for (n = 0; n < possible_cpus->len; n++) {
 Object *cpuobj;
 CPUState *cs;

 if (n >= smp_cpus) {
 break;
 }

 cpuobj = object_new(possible_cpus->cpus[n].type);
 object_property_set_int(cpuobj, "mp-affinity",
 possible_cpus->cpus[n].arch_id, NULL);

 cs = CPU(cpuobj);
 cs->cpu_index = n;

 numa_cpu_pre_plug(&possible_cpus->cpus[cs->cpu_index], DEVICE(cpuobj),
 &error_fatal);

 aarch64 &= object_property_get_bool(cpuobj, "aarch64", NULL);

 if (!vms->secure) {
 object_property_set_bool(cpuobj, "has_el3", false, NULL);
 }

 if (!vms->virt && object_property_find(cpuobj, "has_el2")) {
 object_property_set_bool(cpuobj, "has_el2", false, NULL);
 }

 if (false && object_property_find(cpuobj, "kvm-no-adjvtime")) {
 object_property_set_bool(cpuobj, "kvm-no-adjvtime", true, NULL);
 }

 if (false && object_property_find(cpuobj, "kvm-steal-time")) {
 object_property_set_bool(cpuobj, "kvm-steal-time", false, NULL);
 }

 if (vmc->no_pmu && object_property_find(cpuobj, "pmu")) {
 object_property_set_bool(cpuobj, "pmu", false, NULL);
 }

 if (vmc->no_tcg_lpa2 && object_property_find(cpuobj, "lpa2")) {
 object_property_set_bool(cpuobj, "lpa2", false, NULL);
 }

 if (object_property_find(cpuobj, "reset-cbar")) {
 object_property_set_int(cpuobj, "reset-cbar",
 vms->memmap[VIRT_CPUPERIPHS].base,
 &error_abort);
 }

 object_property_set_link(cpuobj, "memory", OBJECT(sysmem),
 &error_abort);
 if (vms->secure) {
 object_property_set_link(cpuobj, "secure-memory",
 OBJECT(secure_sysmem), &error_abort);
 }

 if (vms->mte) {
 if (tcg_enabled()) {

 if (!tag_sysmem) {

 if (!object_property_find(cpuobj, "tag-memory")) {
 error_report("MTE requested, but not supported "
 "by the guest CPU");
 exit(1);
 }

 tag_sysmem = g_new(MemoryRegion, 1);
 memory_region_init(tag_sysmem, OBJECT(machine),
 "tag-memory", UINT64_MAX / 32);

 if (vms->secure) {
 secure_tag_sysmem = g_new(MemoryRegion, 1);
 memory_region_init(secure_tag_sysmem, OBJECT(machine),
 "secure-tag-memory",
 UINT64_MAX / 32);

 memory_region_add_subregion_overlap(secure_tag_sysmem,
 0, tag_sysmem, -1);
 }
 }

 object_property_set_link(cpuobj, "tag-memory",
 OBJECT(tag_sysmem), &error_abort);
 if (vms->secure) {
 object_property_set_link(cpuobj, "secure-tag-memory",
 OBJECT(secure_tag_sysmem),
 &error_abort);
 }
 } else {
 error_report("MTE requested, but not supported ");
 exit(1);
 }
 }

 qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
 object_unref(cpuobj);
 }

 vms->ns_el2_virt_timer_irq = ns_el2_virt_timer_present() &&
 !vmc->no_ns_el2_virt_timer_irq;

 fdt_add_timer_nodes(vms);
 fdt_add_cpu_nodes(vms);

 memory_region_add_subregion(sysmem, vms->memmap[VIRT_MEM].base,
 machine->ram);

 create_gic(vms, sysmem);

 virt_cpu_post_init(vms, sysmem);

 if (!gunyah_enabled()) {
 fdt_add_pmu_nodes(vms);
 }

 if (!vms->secure) {
 Chardev *serial1 = serial_hd(1);

 if (serial1) {
 vms->second_ns_uart_present = true;
 create_uart(vms, VIRT_UART1, sysmem, serial1, false);
 }
 }
 create_uart(vms, VIRT_UART0, sysmem, serial_hd(0), false);
 if (vms->secure) {
 create_uart(vms, VIRT_UART1, secure_sysmem, serial_hd(1), true);
 }

 if (gunyah_enabled() && !vms->secure) {
 DeviceState *ns_dev;
 SysBusDevice *ns_sbd;

 ns_dev = qdev_new(TYPE_SERIAL_MM);
 qdev_prop_set_uint8(ns_dev, "regshift", 2);
 qdev_prop_set_uint32(ns_dev, "baudbase", 115200);
 qdev_prop_set_uint8(ns_dev, "endianness", DEVICE_LITTLE_ENDIAN);

 if (serial_hd(1)) {
 qdev_prop_set_chr(ns_dev, "chardev", serial_hd(1));
 }
 sysbus_realize_and_unref(SYS_BUS_DEVICE(ns_dev), &error_fatal);
 ns_sbd = SYS_BUS_DEVICE(ns_dev);
 memory_region_add_subregion(sysmem,
 vms->memmap[VIRT_UART1].base,
 sysbus_mmio_get_region(ns_sbd, 0));
 sysbus_connect_irq(ns_sbd, 0,
 qdev_get_gpio_in(vms->gic, vms->irqmap[VIRT_UART1]));
 }

 if (vms->secure) {
 create_secure_ram(vms, secure_sysmem, secure_tag_sysmem);
 }

 if (tag_sysmem) {
 create_tag_ram(tag_sysmem, vms->memmap[VIRT_MEM].base,
 machine->ram_size, "mach-virt.tag");
 }

 vms->highmem_ecam &= (!firmware_loaded || aarch64);

 create_rtc(vms);

 create_pcie(vms);

 if (false) {
 create_gpio_devices(vms, VIRT_GPIO, sysmem);
 }

 if (vms->secure && !vmc->no_secure_gpio) {
 if (false) {
 create_gpio_devices(vms, VIRT_SECURE_GPIO, secure_sysmem);
 }
 }

 vms->powerdown_notifier.notify = virt_powerdown_req;
 qemu_register_powerdown_notifier(&vms->powerdown_notifier);

 vms->fw_cfg = create_fw_cfg(vms, &address_space_memory);
 rom_set_fw(vms->fw_cfg);

 create_platform_bus(vms);

 vms->bootinfo.ram_size = machine->ram_size;
 vms->bootinfo.board_id = -1;
 vms->bootinfo.loader_start = vms->memmap[VIRT_MEM].base;
 vms->bootinfo.get_dtb = machvirt_dtb;
 vms->bootinfo.skip_dtb_autoload = true;
 vms->bootinfo.firmware_loaded = firmware_loaded;
 vms->bootinfo.psci_conduit = vms->psci_conduit;
 if (gunyah_enabled()) {
 vms->bootinfo.modify_dtb = virt_modify_dtb;
 fdt_add_reserved_memory(vms);
 }
 arm_load_kernel(ARM_CPU(first_cpu), machine, &vms->bootinfo);

 if (gunyah_enabled() && vms->bootinfo.entry) {
 GUNYAHState *gs = get_gunyah_state();
 gs->kernel_entry = vms->bootinfo.entry;
 gh_report("kernel entry from arm_load_kernel: 0x%"PRIx64,
 gs->kernel_entry);
 }

 vms->machine_done.notify = virt_machine_done;
 qemu_add_machine_init_done_notifier(&vms->machine_done);
}

static bool virt_get_secure(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->secure;
}

static void virt_set_secure(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->secure = value;
}

static bool virt_get_virt(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->virt;
}

static void virt_set_virt(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->virt = value;
}

static bool virt_get_highmem(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->highmem;
}

static void virt_set_highmem(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->highmem = value;
}

static bool virt_get_compact_highmem(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->highmem_compact;
}

static void virt_set_compact_highmem(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->highmem_compact = value;
}

static bool virt_get_highmem_redists(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->highmem_redists;
}

static void virt_set_highmem_redists(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->highmem_redists = value;
}

static bool virt_get_highmem_ecam(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->highmem_ecam;
}

static void virt_set_highmem_ecam(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->highmem_ecam = value;
}

static bool virt_get_highmem_mmio(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->highmem_mmio;
}

static void virt_set_highmem_mmio(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->highmem_mmio = value;
}

static void virt_get_highmem_mmio_size(Object *obj, Visitor *v,
 const char *name, void *opaque,
 Error **errp)
{
 uint64_t size = extended_memmap[VIRT_HIGH_PCIE_MMIO].size;

 visit_type_size(v, name, &size, errp);
}

static void virt_set_highmem_mmio_size(Object *obj, Visitor *v,
 const char *name, void *opaque,
 Error **errp)
{
 uint64_t size;

 if (!visit_type_size(v, name, &size, errp)) {
 return;
 }

 if (!is_power_of_2(size)) {
 error_setg(errp, "highmem-mmio-size is not a power of 2");
 return;
 }

 if (size < DEFAULT_HIGH_PCIE_MMIO_SIZE) {
 char *sz = size_to_str(DEFAULT_HIGH_PCIE_MMIO_SIZE);
 error_setg(errp, "highmem-mmio-size cannot be set to a lower value "
 "than the default (%s)", sz);
 g_free(sz);
 return;
 }

 extended_memmap[VIRT_HIGH_PCIE_MMIO].size = size;
}

static bool virt_get_its(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->its;
}

static void virt_set_its(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->its = value;
}

static bool virt_get_dtb_randomness(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->dtb_randomness;
}

static void virt_set_dtb_randomness(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->dtb_randomness = value;
}

static void virt_get_acpi(Object *obj, Visitor *v, const char *name,
 void *opaque, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);
 OnOffAuto acpi = vms->acpi;

 visit_type_OnOffAuto(v, name, &acpi, errp);
}

static void virt_set_acpi(Object *obj, Visitor *v, const char *name,
 void *opaque, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 visit_type_OnOffAuto(v, name, &vms->acpi, errp);
}

static bool virt_get_ras(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->ras;
}

static void virt_set_ras(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->ras = value;
}

static bool virt_get_mte(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->mte;
}

static void virt_set_mte(Object *obj, bool value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->mte = value;
}

static char *virt_get_gic_version(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);
 const char *val;

 switch (vms->gic_version) {
 case VIRT_GIC_VERSION_4:
 val = "4";
 break;
 case VIRT_GIC_VERSION_3:
 val = "3";
 break;
 default:
 val = "2";
 break;
 }
 return g_strdup(val);
}

static void virt_set_gic_version(Object *obj, const char *value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 if (!strcmp(value, "4")) {
 vms->gic_version = VIRT_GIC_VERSION_4;
 } else if (!strcmp(value, "3")) {
 vms->gic_version = VIRT_GIC_VERSION_3;
 } else if (!strcmp(value, "2")) {
 vms->gic_version = VIRT_GIC_VERSION_2;
 } else if (!strcmp(value, "host")) {
 vms->gic_version = VIRT_GIC_VERSION_HOST;
 } else if (!strcmp(value, "max")) {
 vms->gic_version = VIRT_GIC_VERSION_MAX;
 } else {
 error_setg(errp, "Invalid gic-version value");
 error_append_hint(errp, "Valid values are 3, 2, host, max.\n");
 }
}

static char *virt_get_iommu(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 switch (vms->iommu) {
 case VIRT_IOMMU_NONE:
 return g_strdup("none");
 case VIRT_IOMMU_SMMUV3:
 return g_strdup("smmuv3");
 default:
 g_assert_not_reached();
 }
}

static void virt_set_iommu(Object *obj, const char *value, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 if (!strcmp(value, "smmuv3")) {
 vms->iommu = VIRT_IOMMU_SMMUV3;
 } else if (!strcmp(value, "none")) {
 vms->iommu = VIRT_IOMMU_NONE;
 } else {
 error_setg(errp, "Invalid iommu value");
 error_append_hint(errp, "Valid values are none, smmuv3.\n");
 }
}

static bool virt_get_default_bus_bypass_iommu(Object *obj, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 return vms->default_bus_bypass_iommu;
}

static void virt_set_default_bus_bypass_iommu(Object *obj, bool value,
 Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);

 vms->default_bus_bypass_iommu = value;
}

static CpuInstanceProperties
virt_cpu_index_to_props(MachineState *ms, unsigned cpu_index)
{
 MachineClass *mc = MACHINE_GET_CLASS(ms);
 const CPUArchIdList *possible_cpus = mc->possible_cpu_arch_ids(ms);

 assert(cpu_index < possible_cpus->len);
 return possible_cpus->cpus[cpu_index].props;
}

static int64_t virt_get_default_cpu_node_id(const MachineState *ms, int idx)
{
 int64_t socket_id = ms->possible_cpus->cpus[idx].props.socket_id;

 return socket_id % ms->numa_state->num_nodes;
}

static const CPUArchIdList *virt_possible_cpu_arch_ids(MachineState *ms)
{
 int n;
 unsigned int max_cpus = ms->smp.max_cpus;
 VirtMachineState *vms = VIRT_MACHINE(ms);
 MachineClass *mc = MACHINE_GET_CLASS(vms);

 if (ms->possible_cpus) {
 assert(ms->possible_cpus->len == max_cpus);
 return ms->possible_cpus;
 }

 ms->possible_cpus = g_malloc0(sizeof(CPUArchIdList) +
 sizeof(CPUArchId) * max_cpus);
 ms->possible_cpus->len = max_cpus;
 for (n = 0; n < ms->possible_cpus->len; n++) {
 ms->possible_cpus->cpus[n].type = ms->cpu_type;
 ms->possible_cpus->cpus[n].arch_id =
 virt_cpu_mp_affinity(vms, n);

 assert(!mc->smp_props.dies_supported);
 ms->possible_cpus->cpus[n].props.has_socket_id = true;
 ms->possible_cpus->cpus[n].props.socket_id =
 n / (ms->smp.clusters * ms->smp.cores * ms->smp.threads);
 ms->possible_cpus->cpus[n].props.has_cluster_id = true;
 ms->possible_cpus->cpus[n].props.cluster_id =
 (n / (ms->smp.cores * ms->smp.threads)) % ms->smp.clusters;
 ms->possible_cpus->cpus[n].props.has_core_id = true;
 ms->possible_cpus->cpus[n].props.core_id =
 (n / ms->smp.threads) % ms->smp.cores;
 ms->possible_cpus->cpus[n].props.has_thread_id = true;
 ms->possible_cpus->cpus[n].props.thread_id =
 n % ms->smp.threads;
 }
 return ms->possible_cpus;
}

static void virt_machine_device_pre_plug_cb(HotplugHandler *hotplug_dev,
 DeviceState *dev, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(hotplug_dev);

 if (object_dynamic_cast(OBJECT(dev), TYPE_VIRTIO_IOMMU_PCI)) {
 hwaddr db_start = 0, db_end = 0;
 QList *reserved_regions;
 char *resv_prop_str;

 if (vms->iommu != VIRT_IOMMU_NONE) {
 error_setg(errp, "virt machine does not support multiple IOMMUs");
 return;
 }

 switch (vms->msi_controller) {
 case VIRT_MSI_CTRL_NONE:
 return;
 case VIRT_MSI_CTRL_ITS:

 db_start = base_memmap[VIRT_GIC_ITS].base + 0x10000;
 db_end = base_memmap[VIRT_GIC_ITS].base +
 base_memmap[VIRT_GIC_ITS].size - 1;
 break;
 case VIRT_MSI_CTRL_GICV2M:

 db_start = base_memmap[VIRT_GIC_V2M].base;
 db_end = db_start + base_memmap[VIRT_GIC_V2M].size - 1;
 break;
 }
 resv_prop_str = g_strdup_printf("0x%"PRIx64":0x%"PRIx64":%u",
 db_start, db_end,
 VIRTIO_IOMMU_RESV_MEM_T_MSI);

 reserved_regions = qlist_new();
 qlist_append_str(reserved_regions, resv_prop_str);
 qdev_prop_set_array(dev, "reserved-regions", reserved_regions);
 g_free(resv_prop_str);
 }
}

static void virt_machine_device_plug_cb(HotplugHandler *hotplug_dev,
 DeviceState *dev, Error **errp)
{
 VirtMachineState *vms = VIRT_MACHINE(hotplug_dev);

 if (vms->platform_bus_dev) {
 MachineClass *mc = MACHINE_GET_CLASS(vms);

 if (device_is_dynamic_sysbus(mc, dev)) {
 platform_bus_link_device(PLATFORM_BUS_DEVICE(vms->platform_bus_dev),
 SYS_BUS_DEVICE(dev));
 }
 }

 if (object_dynamic_cast(OBJECT(dev), TYPE_VIRTIO_IOMMU_PCI)) {
 PCIDevice *pdev = PCI_DEVICE(dev);

 vms->iommu = VIRT_IOMMU_VIRTIO;
 vms->virtio_iommu_bdf = pci_get_bdf(pdev);
 create_virtio_iommu_dt_bindings(vms);
 }
}

static void virt_machine_device_unplug_request_cb(HotplugHandler *hotplug_dev,
 DeviceState *dev, Error **errp)
{
 error_setg(errp, "device unplug request for unsupported device"
 " type: %s", object_get_typename(OBJECT(dev)));
}

static void virt_machine_device_unplug_cb(HotplugHandler *hotplug_dev,
 DeviceState *dev, Error **errp)
{
 error_setg(errp, "virt: device unplug for unsupported device"
 " type: %s", object_get_typename(OBJECT(dev)));
}

static HotplugHandler *virt_machine_get_hotplug_handler(MachineState *machine,
 DeviceState *dev)
{
 MachineClass *mc = MACHINE_GET_CLASS(machine);

 if (device_is_dynamic_sysbus(mc, dev) ||
 object_dynamic_cast(OBJECT(dev), TYPE_VIRTIO_IOMMU_PCI)) {
 return HOTPLUG_HANDLER(machine);
 }
 return NULL;
}

static int virt_hvf_get_physical_address_range(MachineState *ms)
{
 VirtMachineState *vms = VIRT_MACHINE(ms);

 int default_ipa_size = hvf_arm_get_default_ipa_bit_size();
 int max_ipa_size = hvf_arm_get_max_ipa_bit_size();

 virt_set_memmap(vms, max_ipa_size);

 int requested_ipa_size = 64 - clz64(vms->highest_gpa);

 if (requested_ipa_size <= default_ipa_size) {
 requested_ipa_size = default_ipa_size;
 } else if (requested_ipa_size <= max_ipa_size) {
 requested_ipa_size = max_ipa_size;
 } else {
 error_report("-m and ,maxmem option values "
 "require an IPA range (%d bits) larger than "
 "the one supported by the host (%d bits)",
 requested_ipa_size, max_ipa_size);
 return -1;
 }

 return requested_ipa_size;
}

static void virt_machine_class_init(ObjectClass *oc, void *data)
{
 MachineClass *mc = MACHINE_CLASS(oc);
 HotplugHandlerClass *hc = HOTPLUG_HANDLER_CLASS(oc);
 static const char * const valid_cpu_types[] = {
#ifdef CONFIG_TCG
 ARM_CPU_TYPE_NAME("cortex-a7"),
 ARM_CPU_TYPE_NAME("cortex-a15"),
#ifdef TARGET_AARCH64
 ARM_CPU_TYPE_NAME("cortex-a35"),
 ARM_CPU_TYPE_NAME("cortex-a55"),
 ARM_CPU_TYPE_NAME("cortex-a72"),
 ARM_CPU_TYPE_NAME("cortex-a76"),
 ARM_CPU_TYPE_NAME("cortex-a710"),
 ARM_CPU_TYPE_NAME("a64fx"),
 ARM_CPU_TYPE_NAME("neoverse-n1"),
 ARM_CPU_TYPE_NAME("neoverse-v1"),
 ARM_CPU_TYPE_NAME("neoverse-n2"),
#endif
#endif
#ifdef TARGET_AARCH64
 ARM_CPU_TYPE_NAME("cortex-a53"),
 ARM_CPU_TYPE_NAME("cortex-a57"),
#if defined(CONFIG_GH)
 ARM_CPU_TYPE_NAME("host"),
#endif
#endif
 ARM_CPU_TYPE_NAME("max"),
 NULL
 };

 mc->init = machvirt_init;

 mc->max_cpus = 512;
#ifdef CONFIG_TPM
 machine_class_allow_dynamic_sysbus_dev(mc, TYPE_TPM_TIS_SYSBUS);
#endif
 mc->block_default_type = IF_VIRTIO;
 mc->no_cdrom = 1;
 mc->pci_allow_0_address = true;

 mc->minimum_page_bits = 12;
 mc->possible_cpu_arch_ids = virt_possible_cpu_arch_ids;
 mc->cpu_index_to_instance_props = virt_cpu_index_to_props;
#ifdef CONFIG_TCG
 mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a15");
#else
 mc->default_cpu_type = ARM_CPU_TYPE_NAME("max");
#endif
 mc->valid_cpu_types = valid_cpu_types;
 mc->get_default_cpu_node_id = virt_get_default_cpu_node_id;

 mc->hvf_get_physical_address_range = virt_hvf_get_physical_address_range;
 assert(!mc->get_hotplug_handler);
 mc->get_hotplug_handler = virt_machine_get_hotplug_handler;
 hc->pre_plug = virt_machine_device_pre_plug_cb;
 hc->plug = virt_machine_device_plug_cb;
 hc->unplug_request = virt_machine_device_unplug_request_cb;
 hc->unplug = virt_machine_device_unplug_cb;
 mc->smp_props.clusters_supported = true;
 mc->auto_enable_numa_with_memdev = true;

 mc->cpu_cluster_has_numa_boundary = true;
 mc->default_ram_id = "mach-virt.ram";
 mc->default_nic = "virtio-net-pci";

 object_class_property_add(oc, "acpi", "OnOffAuto",
 virt_get_acpi, virt_set_acpi,
 NULL, NULL);
 object_class_property_set_description(oc, "acpi",
 "Enable ACPI");
 object_class_property_add_bool(oc, "secure", virt_get_secure,
 virt_set_secure);
 object_class_property_set_description(oc, "secure",
 "Set on/off to enable/disable the ARM "
 "Security Extensions (TrustZone)");

 object_class_property_add_bool(oc, "virtualization", virt_get_virt,
 virt_set_virt);
 object_class_property_set_description(oc, "virtualization",
 "Set on/off to enable/disable emulating a "
 "guest CPU which implements the ARM "
 "Virtualization Extensions");

 object_class_property_add_bool(oc, "highmem", virt_get_highmem,
 virt_set_highmem);
 object_class_property_set_description(oc, "highmem",
 "Set on/off to enable/disable using "
 "physical address space above 32 bits");

 object_class_property_add_bool(oc, "compact-highmem",
 virt_get_compact_highmem,
 virt_set_compact_highmem);
 object_class_property_set_description(oc, "compact-highmem",
 "Set on/off to enable/disable compact "
 "layout for high memory regions");

 object_class_property_add_bool(oc, "highmem-redists",
 virt_get_highmem_redists,
 virt_set_highmem_redists);
 object_class_property_set_description(oc, "highmem-redists",
 "Set on/off to enable/disable high "
 "memory region for GICv3 or GICv4 "
 "redistributor");

 object_class_property_add_bool(oc, "highmem-ecam",
 virt_get_highmem_ecam,
 virt_set_highmem_ecam);
 object_class_property_set_description(oc, "highmem-ecam",
 "Set on/off to enable/disable high "
 "memory region for PCI ECAM");

 object_class_property_add_bool(oc, "highmem-mmio",
 virt_get_highmem_mmio,
 virt_set_highmem_mmio);
 object_class_property_set_description(oc, "highmem-mmio",
 "Set on/off to enable/disable high "
 "memory region for PCI MMIO");

 object_class_property_add(oc, "highmem-mmio-size", "size",
 virt_get_highmem_mmio_size,
 virt_set_highmem_mmio_size,
 NULL, NULL);
 object_class_property_set_description(oc, "highmem-mmio-size",
 "Set the high memory region size "
 "for PCI MMIO");

 object_class_property_add_str(oc, "gic-version", virt_get_gic_version,
 virt_set_gic_version);
 object_class_property_set_description(oc, "gic-version",
 "Set GIC version. "
 "Valid values are 2, 3, 4, host and max");

 object_class_property_add_str(oc, "iommu", virt_get_iommu, virt_set_iommu);
 object_class_property_set_description(oc, "iommu",
 "Set the IOMMU type. "
 "Valid values are none and smmuv3");

 object_class_property_add_bool(oc, "default-bus-bypass-iommu",
 virt_get_default_bus_bypass_iommu,
 virt_set_default_bus_bypass_iommu);
 object_class_property_set_description(oc, "default-bus-bypass-iommu",
 "Set on/off to enable/disable "
 "bypass_iommu for default root bus");

 object_class_property_add_bool(oc, "ras", virt_get_ras,
 virt_set_ras);
 object_class_property_set_description(oc, "ras",
 "Set on/off to enable/disable reporting host memory errors "
 "to a KVM guest using ACPI and guest external abort exceptions");

 object_class_property_add_bool(oc, "mte", virt_get_mte, virt_set_mte);
 object_class_property_set_description(oc, "mte",
 "Set on/off to enable/disable emulating a "
 "guest CPU which implements the ARM "
 "Memory Tagging Extension");

 object_class_property_add_bool(oc, "its", virt_get_its,
 virt_set_its);
 object_class_property_set_description(oc, "its",
 "Set on/off to enable/disable "
 "ITS instantiation");

 object_class_property_add_bool(oc, "dtb-randomness",
 virt_get_dtb_randomness,
 virt_set_dtb_randomness);
 object_class_property_set_description(oc, "dtb-randomness",
 "Set off to disable passing random or "
 "non-deterministic dtb nodes to guest");

 object_class_property_add_bool(oc, "dtb-kaslr-seed",
 virt_get_dtb_randomness,
 virt_set_dtb_randomness);
 object_class_property_set_description(oc, "dtb-kaslr-seed",
 "Deprecated synonym of dtb-randomness");

}

static void virt_instance_init(Object *obj)
{
 VirtMachineState *vms = VIRT_MACHINE(obj);
 VirtMachineClass *vmc = VIRT_MACHINE_GET_CLASS(vms);

 vms->secure = false;

 vms->virt = false;

 vms->highmem = true;
 vms->highmem_compact = !vmc->no_highmem_compact;
 vms->gic_version = VIRT_GIC_VERSION_NOSEL;

 vms->highmem_ecam = !vmc->no_highmem_ecam;
 vms->highmem_mmio = true;
 vms->highmem_redists = true;

 if (vmc->no_its) {
 vms->its = false;
 } else {

 vms->its = true;

 if (vmc->no_tcg_its) {
 vms->tcg_its = false;
 } else {
 vms->tcg_its = true;
 }
 }

 vms->iommu = VIRT_IOMMU_NONE;

 vms->default_bus_bypass_iommu = false;

 vms->ras = false;

 vms->mte = false;

 vms->dtb_randomness = true;

 vms->irqmap = a15irqmap;

}

static const TypeInfo virt_machine_info = {
 .name = TYPE_VIRT_MACHINE,
 .parent = TYPE_MACHINE,
 .abstract = true,
 .instance_size = sizeof(VirtMachineState),
 .class_size = sizeof(VirtMachineClass),
 .class_init = virt_machine_class_init,
 .instance_init = virt_instance_init,
 .interfaces = (InterfaceInfo[]) {
 { TYPE_HOTPLUG_HANDLER },
 { }
 },
};

static void machvirt_machine_init(void)
{
 type_register_static(&virt_machine_info);
}
type_init(machvirt_machine_init);

static void virt_machine_26_7_options(MachineClass *mc)
{
}
DEFINE_VIRT_MACHINE_AS_LATEST(26, 7)

