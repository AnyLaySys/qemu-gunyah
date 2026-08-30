static int gunyah_handle_mmio_exit(CPUState *cpu, struct gh_vcpu_run *run) {
  uint64_t mmio_addr = run->mmio.phys_addr;
  if ((mmio_addr >= 0x09000000 && mmio_addr < 0x09001000) ||
      (mmio_addr >= 0x3f8 && mmio_addr < 0x400)) {
    if (mmio_addr >= 0x3f8 && mmio_addr < 0x400) {
      uint32_t reg_offset = mmio_addr - 0x3f8;
      if (run->mmio.is_write && reg_offset == 0) {
        bql_lock();
        address_space_rw(&address_space_memory, 0x09000000,
                         MEMTXATTRS_UNSPECIFIED, run->mmio.data, run->mmio.len,
                         true);
        bql_unlock();
      } else if (!run->mmio.is_write && reg_offset == 5) {
        memset(run->mmio.data, 0, run->mmio.len);
        run->mmio.data[0] = 0x60;
      } else {
        memset(run->mmio.data, 0, run->mmio.len);
      }
    } else {
      bql_lock();
      address_space_rw(&address_space_memory, mmio_addr, MEMTXATTRS_UNSPECIFIED,
                       run->mmio.data, run->mmio.len, run->mmio.is_write);
      bql_unlock();
    }
  } else {
    bql_lock();
    address_space_rw(&address_space_memory, mmio_addr,
                     MEMTXATTRS_UNSPECIFIED, run->mmio.data, run->mmio.len,
                     run->mmio.is_write);
    bql_unlock();
  }
  return 0;
}
