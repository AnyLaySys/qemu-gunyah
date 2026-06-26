
#include "qemu/osdep.h"
#include "monitor/monitor.h"
#include "monitor/hmp.h"
#include "qapi/qapi-commands-pci.h"
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"

bool msi_nonbroken;
bool pci_available;

PciInfoList *qmp_query_pci(Error **errp)
{
    return NULL;
}

void hmp_info_pci(Monitor *mon, const QDict *qdict)
{
}

void hmp_pcie_aer_inject_error(Monitor *mon, const QDict *qdict)
{
    monitor_printf(mon, "PCI devices not supported\n");
}

MSIMessage pci_get_msi_message(PCIDevice *dev, int vector)
{
    g_assert_not_reached();
}

uint16_t pci_requester_id(PCIDevice *dev)
{
    g_assert_not_reached();
}

bool msi_enabled(const PCIDevice *dev)
{
    return false;
}

void msi_notify(PCIDevice *dev, unsigned int vector)
{
    g_assert_not_reached();
}

bool msi_is_masked(const PCIDevice *dev, unsigned vector)
{
    g_assert_not_reached();
}

MSIMessage msi_get_message(PCIDevice *dev, unsigned int vector)
{
    g_assert_not_reached();
}

int msix_enabled(PCIDevice *dev)
{
    return false;
}

bool msix_is_masked(PCIDevice *dev, unsigned vector)
{
    g_assert_not_reached();
}

MSIMessage msix_get_message(PCIDevice *dev, unsigned int vector)
{
    g_assert_not_reached();
}
