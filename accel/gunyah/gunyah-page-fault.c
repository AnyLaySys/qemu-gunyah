static int gunyah_handle_page_fault(CPUState *cpu, struct gh_vcpu_run *run) {
  uint64_t fault_addr = run->page_fault.phys_addr;
  int32_t attempt = run->page_fault.attempt;
  if (qatomic_read(&cpu->exit_request) || qatomic_read(&gunyah_vm_stopped)) {
    return EXCP_INTERRUPT;
  }
  gh_report("CPU %d: Page fault at 0x%" PRIx64
            " (attempt=%d) - injecting abort into guest",
            cpu->cpu_index, fault_addr, attempt);
  run->page_fault.resume_action = GH_VCPU_RESUME_FAULT;
  return 0;
}
