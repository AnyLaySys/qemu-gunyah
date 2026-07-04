static bool gunyah_vm_stopped;
static void gunyah_dump_vm_state(GUNYAHState *s) {
  int i;
  error_report("fd=%d vmfd=%d protected=%d swiotlb_size=0x%" PRIx64, s->fd,
               s->vmfd, s->protected_vm, s->swiotlb_size);
  error_report("nr_slots=%u preshmem_reserved=%d", s->nr_slots,
               s->preshmem_reserved);
  for (i = 0; i < s->nr_slots; ++i) {
    if (s->slots[i].size == 0) {
      continue;
    }
    error_report("slot%d gpa=0x%" PRIx64 " size=0x%" PRIx64
                 " hva=%p lend=%d id=%u flags=0x%x",
                 i, s->slots[i].start, s->slots[i].size, s->slots[i].mem,
                 s->slots[i].lend, s->slots[i].id, s->slots[i].flags);
    if (s->slots[i].start <= 0x0f000000 &&
        s->slots[i].start + s->slots[i].size > 0x0f000000) {
      gh_report("ramfb slot present: slot=%d gpa=0x%" PRIx64 " size=0x%" PRIx64
                " lend=%d flags=0x%x",
                i, s->slots[i].start, s->slots[i].size, s->slots[i].lend,
                s->slots[i].flags);
    }
  }
}
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
      gh_report("cpu %d: PSCI SYSTEM_RESET — "
                "Gunyah VMs cannot reset (LEND'd memory "
                "is host-inaccessible), shutting down",
                cpu->cpu_index);
      _exit(0);
    case GH_RM_EXIT_TYPE_VM_EXIT:
    case GH_RM_EXIT_TYPE_PSCI_POWER_OFF:
    default:
      gh_report("cpu %d: PSCI POWER_OFF / VM_EXIT "
                "(exit_type=%d) — exiting",
                cpu->cpu_index, exit_type);
      _exit(0);
    }
  }
}
void gunyah_start_vm(void) {
  int ret, i;
  GUNYAHState *s = GUNYAH_STATE(current_accel());
  CPUState *cpu;
  gunyah_dump_vm_state(s);
  gunyah_cache_lend_range();
  if (!s->protected_vm) {
    gh_report("*** WARNING: protected_vm=false ***");
    gh_report("This means ALL memory uses GH_VM_SET_USER_MEM_REGION "
              "(SHARE), NOT GH_VM_ANDROID_LEND_USER_MEM (LEND).");
    gh_report("--protected-vm-without-firmware uses LEND.");
    gh_report("If this fails, try: -accel gunyah,protected=on");
  } else {
    gh_report("protected_vm=true: memory split into LEND + SHARE "
              "(swiotlb=0x%" PRIx64 ")",
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
    gh_report("creating vCPU %d (BEFORE VM_START, matching strace)",
              vcpu_arg.id);
    ghdbg_hexdump("VCPU gh_fn_desc", &fdesc, sizeof(fdesc));
    ghdbg_hexdump("VCPU gh_fn_vcpu_arg", &vcpu_arg, sizeof(vcpu_arg));
    ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
    if (ret < 0) {
      error_report("could not create VCPU %d: %s (errno=%d)", vcpu_arg.id,
                   strerror(errno), errno);
      exit(1);
    }
    gh_report("vCPU %d created, fd=%d", vcpu_arg.id, ret);
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
        {0x3, GH_IRQFD_FLAGS_LEVEL},
        {0x4, GH_IRQFD_FLAGS_LEVEL},
        {0x5, GH_IRQFD_FLAGS_LEVEL},
        {0x6, GH_IRQFD_FLAGS_LEVEL},
        {0xf, 0},
    };
    int nbell = sizeof(bells) / sizeof(bells[0]);
    EventNotifier pl011_notifier;
    int pl011_notifier_valid = 0;
#define PCI_FIRST_SPI 3
#define PCI_NUM_SPIS 4
    EventNotifier pci_notifiers[PCI_NUM_SPIS];
    gh_report("creating %d IRQFDs (base + PCI)", nbell);
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
      if (bells[i].label >= PCI_FIRST_SPI &&
          bells[i].label < PCI_FIRST_SPI + PCI_NUM_SPIS) {
        int idx = bells[i].label - PCI_FIRST_SPI;
        event_notifier_init_fd(&pci_notifiers[idx], efd);
      }
      ghirqfd.fd = efd;
      ghirqfd.label = bells[i].label;
      ghirqfd.flags = bells[i].flags;
      ghirqfd.padding = 0;
      fdesc.type = GH_FN_IRQFD;
      fdesc.arg_size = sizeof(struct gh_fn_irqfd_arg);
      fdesc.arg = (__u64)(&ghirqfd);
      gh_report("IRQFD bell-%x label=%d efd=%d flags=0x%x", bells[i].label,
                bells[i].label, efd, bells[i].flags);
      ghdbg_hexdump("IRQFD gh_fn_desc", &fdesc, sizeof(fdesc));
      ghdbg_hexdump("IRQFD gh_fn_irqfd_arg", &ghirqfd, sizeof(ghirqfd));
      ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
      if (ret != 0) {
        gh_report("IRQFD bell-%x FAILED: %s (errno=%d)", bells[i].label,
                  strerror(errno), errno);
      } else {
        gh_report("IRQFD bell-%x OK", bells[i].label);
      }
    }
    if (pl011_notifier_valid) {
      gunyah_gic_register_irq_notifiers(&pl011_notifier, 1, 1);
      gh_report("PL011 GIC notifier registered for SPI 1");
    }
    gunyah_gic_register_irq_notifiers(pci_notifiers, PCI_NUM_SPIS,
                                      PCI_FIRST_SPI);
#undef PCI_FIRST_SPI
#undef PCI_NUM_SPIS
  }
  {
#define NUM_VIRTIO_BELLS 32
    EventNotifier virtio_notifiers[NUM_VIRTIO_BELLS];
    int virtio_ok = 0;
    gh_report("creating %d virtio IRQFDs (labels 0x10-0x2f, "
              "SPIs 16-47)",
              NUM_VIRTIO_BELLS);
    for (i = 0; i < NUM_VIRTIO_BELLS; i++) {
      struct gh_fn_desc fdesc;
      struct gh_fn_irqfd_arg ghirqfd = {0};
      int efd;
      int label = 0x10 + i;
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
    gh_report("%d/%d virtio IRQFDs created OK", virtio_ok, NUM_VIRTIO_BELLS);
    gunyah_gic_register_irq_notifiers(virtio_notifiers, NUM_VIRTIO_BELLS, 16);
#undef NUM_VIRTIO_BELLS
  }
  if (s->dtb_start) {
    struct gh_vm_dtb_config dtb;
    dtb.guest_phys_addr = s->dtb_start;
    dtb.size = s->dtb_size;
    gh_report("SET_DTB_CONFIG gpa=0x%" PRIx64 " size=0x%" PRIx64,
              (uint64_t)dtb.guest_phys_addr, (uint64_t)dtb.size);
    ghdbg_hexdump("SET_DTB_CONFIG gh_vm_dtb_config", &dtb, sizeof(dtb));
    for (i = 0; i < s->nr_slots; ++i) {
      if (s->slots[i].size == 0)
        continue;
      uint64_t slot_end = s->slots[i].start + s->slots[i].size;
      if (dtb.guest_phys_addr >= s->slots[i].start &&
          dtb.guest_phys_addr < slot_end) {
        gh_report("DTB falls in slot[%d] gpa=0x%" PRIx64 " size=0x%" PRIx64
                  " lend=%d (offset=0x%" PRIx64 ")",
                  i, s->slots[i].start, s->slots[i].size, s->slots[i].lend,
                  (uint64_t)(dtb.guest_phys_addr - s->slots[i].start));
      }
    }
    for (i = 0; i < s->nr_slots; ++i) {
      if (s->slots[i].size == 0)
        continue;
      uint64_t slot_end = s->slots[i].start + s->slots[i].size;
      if (dtb.guest_phys_addr >= s->slots[i].start &&
          dtb.guest_phys_addr < slot_end) {
        uint64_t offset = dtb.guest_phys_addr - s->slots[i].start;
        void *dtb_hva = (uint8_t *)s->slots[i].mem + offset;
        gh_report("DTB content at HVA %p (slot[%d] + 0x%" PRIx64 "):", dtb_hva,
                  i, offset);
        ghdbg_hexdump("DTB first 64 bytes", dtb_hva, 64);
        uint32_t magic = *(uint32_t *)dtb_hva;
        gh_report("DTB magic=0x%08x (%s)", magic,
                  magic == 0xd00dfeed   ? "VALID (big-endian)"
                  : magic == 0xedfe0dd0 ? "VALID (needs swap)"
                                        : "INVALID!");
        break;
      }
    }
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
      gh_report("Using kernel_entry=0x%" PRIx64 " from arm_load_kernel()",
                kernel_entry);
    } else if (kernel_hva_base) {
      uint32_t *insns = (uint32_t *)kernel_hva_base;
      if (insns[1] == 0xaa1f03e1 && insns[2] == 0xaa1f03e2 &&
          insns[3] == 0xaa1f03e3) {
        uint64_t wrapper_entry =
            *(uint64_t *)((uint8_t *)kernel_hva_base + 0x20);
        gh_report("Detected boot wrapper at 0x%" PRIx64, kernel_load_addr);
        kernel_entry = wrapper_entry;
        gh_report("Using real kernel entry 0x%" PRIx64 " (skipping wrapper)",
                  kernel_entry);
      } else {
        gh_report("WARNING: kernel_entry not set by machine "
                  "and no wrapper detected, "
                  "using kernel_entry=0x%" PRIx64,
                  kernel_entry);
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
      gh_report("SET_BOOT_CONTEXT PC=0x%" PRIx64, (uint64_t)boot_ctx.value);
      ghdbg_hexdump("SET_BOOT_CONTEXT gh_vm_boot_context", &boot_ctx,
                    sizeof(boot_ctx));
      ret = gunyah_vm_ioctl(GH_VM_SET_BOOT_CONTEXT, &boot_ctx);
      if (ret != 0) {
        if (errno == ENOTTY) {
          gh_report("SET_BOOT_CONTEXT not supported (ENOTTY)");
        } else {
          gh_report("SET_BOOT_CONTEXT PC failed: %s (errno=%d)",
                    strerror(errno), errno);
        }
      } else {
        gh_report("SET_BOOT_CONTEXT PC OK");
      }
    }
    gh_report("skipping X0 (RM sets X0=DTB from SET_DTB_CONFIG)");
  }
  for (i = 0; i < s->nr_slots; ++i) {
    if (s->slots[i].size > 0 && s->slots[i].mem) {
      gh_report(
          "pre-VM_START slot[%d] gpa=0x%" PRIx64 " lend=%d first 32 bytes:", i,
          s->slots[i].start, s->slots[i].lend);
      ghdbg_hexdump("slot content", s->slots[i].mem, 32);
    }
  }
  gunyah_install_sigsegv_handler();
  ret = gunyah_vm_ioctl(GH_VM_START);
  if (ret != 0) {
    gh_report("Failed to start VM:%s (errno=%d)", strerror(errno), errno);
    exit(1);
  }
  gh_report("VM_START OK");
  sigbus_lend_count = 0;
  for (i = 0; i < s->nr_slots; i++) {
    gunyah_slot *slot = &s->slots[i];
    if (slot->size && slot->lend && slot->mem) {
      if (sigbus_lend_count < SIGBUS_MAX_LEND_REGIONS) {
        sigbus_lend_regions[sigbus_lend_count].hva = slot->mem;
        sigbus_lend_regions[sigbus_lend_count].size = slot->size;
        sigbus_lend_regions[sigbus_lend_count].gpa = slot->start;
        sigbus_lend_count++;
      }
      gh_report("LEND region hva=%p size=0x%zx kept mapped "
                "for demand paging",
                slot->mem, (size_t)slot->size);
    }
  }
  __sync_synchronize();
  sigbus_handler_active = 1;
  gh_report("SIGBUS handler armed with %d LEND regions", sigbus_lend_count);
  qatomic_set(&s->vm_started, 1);
}