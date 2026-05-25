GUNYAHState *get_gunyah_state(void) {
    return GUNYAH_STATE(current_accel());
}
static void gunyah_ipi_signal(int sig) {
    if (current_cpu) {
        qatomic_set(&current_cpu->accel->run->immediate_exit, 1);
    }
}
static void gunyah_cpu_kick_self(void) {
    qatomic_set(&current_cpu->accel->run->immediate_exit, 1);
}
static int gunyah_init_vcpu(CPUState *cpu, Error **errp) {
    int ret;
    struct sigaction sigact;
    sigset_t set;
    cpu->accel = g_new0(AccelCPUState, 1);
    cpu->accel->fd = -1;
    cpu->accel->run = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cpu->accel->run == MAP_FAILED) {
        error_report("mmap of dummy vcpu run page failed: %s", strerror(errno));
        exit(1);
    }
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = gunyah_ipi_signal;
    sigaction(SIG_IPI, &sigact, NULL);
    pthread_sigmask(SIG_BLOCK, NULL, &set);
    sigdelset(&set, SIG_IPI);
    sigdelset(&set, SIGBUS);
    sigdelset(&set, SIGSEGV);
    ret = pthread_sigmask(SIG_SETMASK, &set, NULL);
    if (ret) {
        error_report("pthread_sigmask: %s", strerror(ret));
        exit(1);
    }
    error_report("gh    │init_vcpu %d (deferred - VCPU will be created at VM start)", cpu->cpu_index);
    return 0;
}
static void gunyah_vcpu_destroy(CPUState *cpu) {
    int ret;
    ret = munmap(cpu->accel->run, 4096);
    if (ret < 0) {
        error_report("munmap of vcpu run structure failed: %s", strerror(errno));
        exit(1);
    }
    close(cpu->accel->fd);
    g_free(cpu->accel);
}
static int gunyah_handle_unknown_exit(CPUState *cpu, struct gh_vcpu_run *run) {
    uint64_t unk_addr = run->mmio.phys_addr;
    AccelCPUState *acpu = cpu->accel;
    if (!gunyah_addr_is_lend(unk_addr) && run->mmio.len > 0 && run->mmio.len <= 8) {
        bql_lock();
        address_space_rw(&address_space_memory, unk_addr, MEMTXATTRS_UNSPECIFIED, run->mmio.data,
                         run->mmio.len, run->mmio.is_write);
        bql_unlock();
        return 0;
    } else {
        if (unk_addr == acpu->last_fault_addr) {
            acpu->same_fault_count++;
        } else {
            acpu->last_fault_addr = unk_addr;
            acpu->same_fault_count = 1;
        }
        if (acpu->same_fault_count > 500) {
            error_report("gh    │CPU#%d exit_reason=%d stuck "
                         "at addr=0x%"
            PRIx64
            " (%d repeats)"
            " — unresolvable fault, aborting", cpu->cpu_index, run->exit_reason, unk_addr, acpu->same_fault_count);
            return -1;
        } else {
            if (acpu->same_fault_count > 10) {
                usleep(100);
            }
            return 0;
        }
    }
}
static int gunyah_vcpu_exec(CPUState *cpu) {
    int ret;
    static bool handler_installed;
    if (cpu->accel->fd < 0) {
        error_report("gh    │ERROR: vcpu fd is %d (invalid!)", cpu->accel->fd);
        return EXCP_INTERRUPT;
    }
    if (!handler_installed) {
        gunyah_install_sigsegv_handler();
        handler_installed = true;
    }
    if (qatomic_read(&gunyah_vm_stopped)) {
        return EXCP_INTERRUPT;
    }
    bql_unlock();
    cpu_exec_start(cpu);
    do {
        struct gh_vcpu_run *run = cpu->accel->run;
        int exit_reason;
        if (qatomic_read(&cpu->exit_request)) {
            gunyah_cpu_kick_self();
        }
        ret = gunyah_vcpu_ioctl(cpu, GH_VCPU_RUN);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                qatomic_set(&run->immediate_exit, 0);
                ret = EXCP_INTERRUPT;
                break;
            }
            if (errno == ENODEV) {
                if (qatomic_read(&gunyah_vm_stopped) || qatomic_read(&cpu->exit_request) ||
                    cpu->cpu_index == 0) {
                    ret = EXCP_INTERRUPT;
                    break;
                }
                usleep(10000);
                ret = 0;
                continue;
            }
            error_report("GH_VCPU_RUN: %s (errno=%d)", strerror(errno), errno);
            if (errno == EBUSY) {
                error_report("gh    │cpu %d: GH_VCPU_RUN returned EBUSY — "
                             "VM shutting down, exiting", cpu->cpu_index);
                qatomic_set(&gunyah_vm_stopped, true);
                _exit(0);
            }
            ret = -1;
            break;
        }
        exit_reason = run->exit_reason;
        switch (exit_reason) {
            case GH_VCPU_EXIT_MMIO:
                ret = gunyah_handle_mmio_exit(cpu, run);
                break;
            case GH_VCPU_EXIT_STATUS:
                gunyah_handle_vm_status(cpu, run);
                ret = EXCP_INTERRUPT;
                break;
            case GH_VCPU_EXIT_PAGE_FAULT:
                ret = gunyah_handle_page_fault(cpu, run);
                break;
            default:
                ret = gunyah_handle_unknown_exit(cpu, run);
        }
    } while (ret == 0);
    cpu_exec_end(cpu);
    bql_lock();
    if (ret < 0) {
        cpu_dump_state(cpu, stderr, CPU_DUMP_CODE);
        vm_stop(RUN_STATE_INTERNAL_ERROR);
    }
    qatomic_set(&cpu->exit_request, 0);
    return ret;
}
void *gunyah_cpu_thread_fn(void *arg) {
    CPUState *cpu = arg;
    rcu_register_thread();
    bql_lock();
    qemu_thread_get_self(cpu->thread);
    cpu->thread_id = qemu_get_thread_id();
    current_cpu = cpu;
    error_report("gh    │cpu_thread_fn started for cpu %d (tid=%d)", cpu->cpu_index, cpu->thread_id);
    gunyah_init_vcpu(cpu, &error_fatal);
    cpu_thread_signal_created(cpu);
    qemu_guest_random_seed_thread_part2(cpu->random_seed);
    error_report("gh    │cpu %d entering main loop (fd=%d run=%p)", cpu->cpu_index, cpu->accel->fd,
                 cpu->accel->run);
    do {
        if (cpu_can_run(cpu)) {
            gunyah_vcpu_exec(cpu);
        }
        qemu_wait_io_event(cpu);
    } while (!cpu->unplug || cpu_can_run(cpu));
    gunyah_vcpu_destroy(cpu);
    cpu_thread_signal_destroyed(cpu);
    bql_unlock();
    rcu_unregister_thread();
    return NULL;
}
static void do_gunyah_cpu_synchronize_post_reset(CPUState *cpu, run_on_cpu_data arg) {
    gunyah_arch_put_registers(cpu, 0);
    cpu->vcpu_dirty = false;
}
void gunyah_cpu_synchronize_post_reset(CPUState *cpu) {
    run_on_cpu(cpu, do_gunyah_cpu_synchronize_post_reset, RUN_ON_CPU_NULL);
}
