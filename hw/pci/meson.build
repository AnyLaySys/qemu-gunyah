pci_ss = ss.source_set()
pci_ss.add(files(
  'msi.c',
  'msix.c',
  'pci.c',
  'pci_bridge.c',
  'pci_host.c',
  'pci-hmp-cmds.c',
  'pcie_sriov.c'
))
pci_ss.add(files('pcie.c', 'pcie_aer.c'))
system_ss.add(when: 'CONFIG_PCI_EXPRESS', if_true: files('pcie_port.c', 'pcie_host.c'))
system_ss.add_all(when: 'CONFIG_PCI', if_true: pci_ss)
