/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PCI_EXPANDER_BRIDGE_H
#define PCI_EXPANDER_BRIDGE_H

typedef struct CXLState CXLState;
typedef struct PCIBus PCIBus;

void pxb_cxl_hook_up_registers(CXLState *state, PCIBus *bus, Error **errp);

#endif /* PCI_EXPANDER_BRIDGE_H */
