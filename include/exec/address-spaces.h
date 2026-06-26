
#ifndef EXEC_ADDRESS_SPACES_H
#define EXEC_ADDRESS_SPACES_H


#ifndef CONFIG_USER_ONLY

MemoryRegion *get_system_memory(void);

MemoryRegion *get_system_io(void);

extern AddressSpace address_space_memory;
extern AddressSpace address_space_io;

#endif

#endif
