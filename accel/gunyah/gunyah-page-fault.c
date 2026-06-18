static int gunyah_handle_page_fault(CPUState *cpu, struct gh_vcpu_run *run) {
    uint64_t fault_addr = run->page_fault.phys_addr;
    int32_t attempt = run->page_fault.attempt;
    AccelCPUState *acpu = cpu->accel;
    if (fault_addr >= 0x0f000000 && fault_addr < 0x10000000) {
        static int ramfb_fault_count;
        if (ramfb_fault_count < 64) {
            gh_report("ramfb PAGE_FAULT #%d: addr=0x%"PRIx64" attempt=%d lend=%d",
                      ramfb_fault_count, fault_addr, attempt,
                      gunyah_addr_is_lend(fault_addr));
        }
        ramfb_fault_count++;
    }
    if (fault_addr == acpu->last_fault_addr) {
        acpu->same_fault_count++;
    } else {
        acpu->last_fault_addr = fault_addr;
        acpu->same_fault_count = 1;
    }
    if (acpu->same_fault_count == 1) {
        static int page_fault_log_count;
        if (page_fault_log_count < 100) {
            gh_report("CPU#%d PAGE_FAULT at "
                         "0x%"
            PRIx64
            " attempt=%d (%s) lend=%d", cpu->cpu_index, fault_addr, attempt, attempt == -2
                                                                             ? "ENOENT" : attempt ==
                                                                                          -1
                                                                                          ? "EPERM"
                                                                                          :
                                                                                          attempt ==
                                                                                          -5 ? "EIO"
                                                                                             :
                                                                                          attempt ==
                                                                                          -12
                                                                                          ? "ENOMEM"
                                                                                          :
                                                                                          attempt ==
                                                                                          -95
                                                                                          ? "EOPNOTSUPP"
                                                                                          : "other", gunyah_addr_is_lend(
                    fault_addr));
            page_fault_log_count++;
        }
    }
    if (qatomic_read(&cpu->exit_request) || qatomic_read(&gunyah_vm_stopped)) {
        return EXCP_INTERRUPT;
    }
    {
        int max_retries = (attempt == -12) ? 10 : 10;
        if (acpu->same_fault_count <= max_retries) {
            run->page_fault.resume_action = GH_VCPU_RESUME_RETRY;
            if (acpu->same_fault_count > 3) {
                usleep(1000);
            }
            return 0;
        } else {
            if (acpu->same_fault_count == max_retries + 1) {
                gh_report("CPU#%d PAGE_FAULT at 0x%"
                PRIx64
                " PERMANENT (attempt=%d) after %d retries"
                " — injecting abort into guest", cpu->cpu_index, fault_addr, attempt, acpu->same_fault_count);
            }
            run->page_fault.resume_action = GH_VCPU_RESUME_FAULT;
            usleep(50000);
            return 0;
        }
    }
}
