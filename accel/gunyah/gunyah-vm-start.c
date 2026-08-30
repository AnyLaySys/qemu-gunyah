bool gunyah_vm_stopped;
static bool gunyah_restart_requested;
static void gunyah_handle_vm_status(CPUState *cpu, struct gh_vcpu_run *run) {
  enum gh_vm_status exit_status = run->status.status;
  enum gh_vm_exit_type exit_type = run->status.exit_info.type;
  qatomic_set(&gunyah_vm_stopped, true);
  switch (exit_status) {
  case GH_VM_STATUS_CRASHED:
    gh_report("cpu %d: VM CRASHED", cpu->cpu_index);
    cpu_exec_end(cpu);
    bql_lock();
    qemu_system_guest_panicked(NULL);
    bql_unlock();
    cpu_exec_start(cpu);
    break;
  case GH_VM_STATUS_EXITED:
  default:
    qatomic_set(&cpu->halted, 1);
    switch (exit_type) {
    case GH_RM_EXIT_TYPE_WDT_BITE:
      gh_report("cpu %d: WDT BITE", cpu->cpu_index);
      cpu_exec_end(cpu);
      bql_lock();
      qemu_system_guest_panicked(NULL);
      bql_unlock();
      cpu_exec_start(cpu);
      break;
    case GH_RM_EXIT_TYPE_PSCI_SYSTEM_RESET:
    case GH_RM_EXIT_TYPE_PSCI_SYSTEM_RESET2:
      if (!qatomic_xchg(&gunyah_restart_requested, true)) {
        gh_report("cpu %d: PSCI SYSTEM_RESET", cpu->cpu_index);
        qemu_system_shutdown_request_with_code(
            SHUTDOWN_CAUSE_GUEST_RESET, GUNYAH_VM_RESTART_STATUS);
      }
      break;
    case GH_RM_EXIT_TYPE_VM_EXIT:
    case GH_RM_EXIT_TYPE_PSCI_POWER_OFF:
    default:
      gh_report("cpu %d: PSCI POWER_OFF / VM_EXIT (exit_type=%d)",
                cpu->cpu_index, exit_type);
      qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
      break;
    }
  }
}
void gunyah_start_vm(void) {
  int ret, i;
  GUNYAHState *s = GUNYAH_STATE(current_accel());
  CPUState *cpu;
  gunyah_cache_lend_range();
  if (!s->protected_vm) {
    gh_report("protected VM disabled");
  } else {
    gh_report("protected VM: LEND + SHARE (swiotlb=0x%" PRIx64 ")",
              s->swiotlb_size);
  }
  CPU_FOREACH(cpu) {
    struct gh_fn_desc fdesc;
    struct gh_fn_vcpu_arg vcpu_arg;
    long mmap_size;
    vcpu_arg.id = cpu->cpu_index;
    fdesc.type = GH_FN_VCPU;
    fdesc.arg_size = sizeof(struct gh_fn_vcpu_arg);
    fdesc.arg = (__u64)(&vcpu_arg);
    ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
    if (ret < 0) {
      error_report("could not create VCPU %d: %s (errno=%d)", vcpu_arg.id,
                   strerror(errno), errno);
      exit(1);
    }
    mmap_size = ioctl(ret, GH_VCPU_MMAP_SIZE);
    if (mmap_size < 0) {
      error_report("GH_VCPU_MMAP_SIZE failed for vCPU %d: %s (errno=%d)",
                   vcpu_arg.id, strerror(errno), errno);
      exit(1);
    }
    if ((size_t)mmap_size < sizeof(*cpu->accel->run)) {
      error_report("GH_VCPU_MMAP_SIZE too small for vCPU %d: %ld < %zu",
                   vcpu_arg.id, mmap_size, sizeof(*cpu->accel->run));
      exit(1);
    }
    munmap(cpu->accel->run, cpu->accel->run_size);
    cpu->accel->fd = ret;
    cpu->accel->run_size = mmap_size;
    cpu->accel->run = mmap(0, cpu->accel->run_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED, ret, 0);
    if (cpu->accel->run == MAP_FAILED) {
      error_report("mmap of vcpu run structure failed: %s", strerror(errno));
      exit(1);
    }
  }
  {
    struct {
      int label;
      int flags;
    } bells[] = {
        {0x0, 0},
        {0x1, GH_IRQFD_FLAGS_LEVEL},
        {0x2, 0},
        {0xf, 0},
    };
    int nbell = sizeof(bells) / sizeof(bells[0]);
    EventNotifier pl011_notifier;
    int pl011_notifier_valid = 0;
    for (i = 0; i < nbell; ++i) {
      struct gh_fn_desc fdesc;
      struct gh_fn_irqfd_arg ghirqfd = {0};
      int efd;
      efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
      if (efd < 0) {
        gh_report("eventfd failed for bell-%x: %s", bells[i].label,
                  strerror(errno));
        exit(1);
      }
      if (bells[i].label == 0x1) {
        event_notifier_init_fd(&pl011_notifier, efd);
        pl011_notifier_valid = 1;
      }
      ghirqfd.fd = efd;
      ghirqfd.label = bells[i].label;
      ghirqfd.flags = bells[i].flags;
      ghirqfd.padding = 0;
      fdesc.type = GH_FN_IRQFD;
      fdesc.arg_size = sizeof(struct gh_fn_irqfd_arg);
      fdesc.arg = (__u64)(&ghirqfd);
      ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
      if (ret != 0) {
        gh_report("IRQFD bell-%x FAILED: %s (errno=%d)", bells[i].label,
                  strerror(errno), errno);
      }
    }
    if (pl011_notifier_valid) {
      gunyah_gic_register_irq_notifiers(&pl011_notifier, 1, 1);
    }
  }
  if (s->msi_vectors) {
    EventNotifier *virtio_notifiers = g_new(EventNotifier, s->msi_vectors);
    int virtio_ok = 0;
    for (i = 0; i < s->msi_vectors; i++) {
      struct gh_fn_desc fdesc;
      struct gh_fn_irqfd_arg ghirqfd = {0};
      int efd;
      int label = GUNYAH_MSI_SPI_BASE + i;
      efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
      if (efd < 0) {
        gh_report("eventfd failed for virtio bell-%x: %s", label,
                  strerror(errno));
        continue;
      }
      event_notifier_init_fd(&virtio_notifiers[i], efd);
      ghirqfd.fd = efd;
      ghirqfd.label = label;
      ghirqfd.flags = 0;
      ghirqfd.padding = 0;
      fdesc.type = GH_FN_IRQFD;
      fdesc.arg_size = sizeof(struct gh_fn_irqfd_arg);
      fdesc.arg = (__u64)(&ghirqfd);
      ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
      if (ret != 0) {
        gh_report("IRQFD virtio bell-%x FAILED: %s (errno=%d)", label,
                  strerror(errno), errno);
      } else {
        virtio_ok++;
      }
    }
    gh_report("%d/%d virtio IRQFDs created OK", virtio_ok,
              s->msi_vectors);
    gunyah_gic_register_irq_notifiers(virtio_notifiers,
                                      s->msi_vectors,
                                      GUNYAH_MSI_SPI_BASE);
    g_free(virtio_notifiers);
  }
  if (s->dtb_start) {
    struct gh_vm_dtb_config dtb;
    dtb.guest_phys_addr = s->dtb_start;
    dtb.size = s->dtb_size;
    ret = gunyah_vm_ioctl(GH_VM_SET_DTB_CONFIG, &dtb);
    if (ret != 0) {
      error_report("GH_VM_SET_DTB_CONFIG failed: %s (errno=%d)",
                   strerror(errno), errno);
      exit(1);
    }
    gh_report("SET_DTB_CONFIG OK");
  }
  {
    uint64_t kernel_entry = 0;
    uint64_t kernel_load_addr = 0;
    void *kernel_hva_base = NULL;
    uint64_t stub_gpa;
    void *stub_hva = NULL;
    for (i = 0; i < s->nr_slots; ++i) {
      if (s->slots[i].size > 0 && s->slots[i].lend) {
        kernel_load_addr = s->slots[i].start;
        kernel_hva_base = s->slots[i].mem;
        break;
      }
    }
    kernel_entry = kernel_load_addr;
    if (s->kernel_entry) {
      kernel_entry = s->kernel_entry;
    } else if (kernel_hva_base) {
      uint32_t *insns = (uint32_t *)kernel_hva_base;
      if (insns[1] == 0xaa1f03e1 && insns[2] == 0xaa1f03e2 &&
          insns[3] == 0xaa1f03e3) {
        uint64_t wrapper_entry =
            *(uint64_t *)((uint8_t *)kernel_hva_base + 0x20);
        kernel_entry = wrapper_entry;
      }
    }
    stub_gpa = s->dtb_start - 0x1000;
    for (i = 0; i < s->nr_slots; ++i) {
      if (s->slots[i].size > 0 && stub_gpa >= s->slots[i].start &&
          stub_gpa < s->slots[i].start + s->slots[i].size) {
        uint64_t offset = stub_gpa - s->slots[i].start;
        stub_hva = s->slots[i].mem + offset;
        break;
      }
    }
    if (stub_hva) {
      uint32_t stub[] = {
          (uint32_t)(0xD2800001 | ((kernel_entry & 0xFFFF) << 5)),
          (uint32_t)(0xF2A00001 | (((kernel_entry >> 16) & 0xFFFF) << 5)),
          0xAA1F03E2, 0xAA1F03E3, 0xD61F0020};
      memcpy(stub_hva, stub, sizeof(stub));
    } else {
      gh_report("WARNING: could not find HVA for stub "
                "GPA=0x%" PRIx64 ", skipping boot stub",
                stub_gpa);
      stub_gpa = kernel_entry;
    }
    {
      struct gh_vm_boot_context boot_ctx = {0};
      boot_ctx.reg =
          (GH_VM_BOOT_CONTEXT_REG_SET_PC << GH_VM_BOOT_CONTEXT_REG_SHIFT) | 0;
      boot_ctx.value = stub_gpa;
      ret = gunyah_vm_ioctl(GH_VM_SET_BOOT_CONTEXT, &boot_ctx);
      if (ret != 0) {
        if (errno == ENOTTY) {
          gh_report("SET_BOOT_CONTEXT not supported (ENOTTY)");
        } else {
          gh_report("SET_BOOT_CONTEXT PC failed: %s (errno=%d)",
                    strerror(errno), errno);
        }
      }
    }
  }
  ret = gunyah_vm_ioctl(GH_VM_START);
  if (ret != 0) {
    gh_report("Failed to start VM:%s (errno=%d)", strerror(errno), errno);
    exit(1);
  }
  gh_report("VM_START OK");
  qatomic_set(&s->vm_started, 1);
}
