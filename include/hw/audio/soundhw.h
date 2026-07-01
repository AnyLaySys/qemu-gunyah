#ifndef HW_SOUNDHW_H
#define HW_SOUNDHW_H

void pci_register_soundhw(const char *name,
                          int (*init_pci)(PCIBus *bus, const char *audiodev));
void deprecated_register_soundhw(const char *name, int isa, const char *typename);

void soundhw_init(void);
void select_soundhw(const char *name, const char *audiodev);

#endif
