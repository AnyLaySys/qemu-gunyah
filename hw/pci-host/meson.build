pci_ss = ss.source_set()
pci_ss.add(when: 'CONFIG_PCI_EXPRESS_GENERIC_BRIDGE', if_true: files('gpex.c'))

system_ss.add_all(when: 'CONFIG_PCI', if_true: pci_ss)
