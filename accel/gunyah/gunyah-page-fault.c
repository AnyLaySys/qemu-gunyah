static int gunyah_handle_page_fault(CPUState *cpu, struct gh_vcpu_run *run) {
  uint64_t fault_addr = run->page_fault.phys_addr;
  int32_t attempt = run->page_fault.attempt;
  AccelCPUState *acpu = cpu->accel;
  if (fault_addr == acpu->last_fault_addr) {
    acpu->same_fault_count++;
  } else {
    acpu->last_fault_addr = fault_addr;
    acpu->same_fault_count = 1;
  }
  if (qatomic_read(&cpu->exit_request) || qatomic_read(&gunyah_vm_stopped)) {
    return EXCP_INTERRUPT;
  }
  {
    int max_retries = 10;
    if (acpu->same_fault_count <= max_retries) {
      run->page_fault.resume_action = GH_VCPU_RESUME_RETRY;
      if (acpu->same_fault_count > 3) {
        usleep(1000);
      }
      return 0;
    } else {
      if (acpu->same_fault_count == max_retries + 1) {
        gh_report("CPU#%d PAGE_FAULT at 0x%" PRIx64
                  " PERMANENT (attempt=%d) after %d retries"
                  " — injecting abort into guest",
                  cpu->cpu_index, fault_addr, attempt, acpu->same_fault_count);
      }
      run->page_fault.resume_action = GH_VCPU_RESUME_FAULT;
      usleep(50000);
      return 0;
    }
  }
}
