static int gunyah_handle_page_fault(CPUState *cpu, struct gunyah_vcpu_run *run)
{
    if (qatomic_read(&cpu->exit_request) ||
        qatomic_read(&gunyah_vm_stopped)) {
        return EXCP_INTERRUPT;
    }

    run->page_fault.resume_action = GUNYAH_VCPU_RESUME_FAULT;
    return 0;
}
