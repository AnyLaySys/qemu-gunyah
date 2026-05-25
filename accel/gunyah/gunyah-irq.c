int gunyah_add_irqfd(int irqfd, int label, Error **errp) {
    int ret;
    struct gh_fn_desc fdesc;
    struct gh_fn_irqfd_arg ghirqfd;
    fdesc.type = GH_FN_IRQFD;
    fdesc.arg_size = sizeof(struct gh_fn_irqfd_arg);
    fdesc.arg = (__u64)(&ghirqfd);
    ghirqfd.fd = irqfd;
    ghirqfd.label = label;
    ghirqfd.flags = GH_IRQFD_FLAGS_LEVEL;
    error_report("gh    │add_irqfd label=%d fd=%d flags=0x%x", label, irqfd, ghirqfd.flags);
    ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
    if (ret) {
        error_report("gh    │add_irqfd FAILED label=%d: %s (errno=%d)", label, strerror(errno), errno);
        error_setg_errno(errp, errno, "GH_FN_IRQFD failed");
    } else {
        error_report("gh    │add_irqfd OK label=%d", label);
    }
    return ret;
}
static int
gunyah_set_ioeventfd_mmio(int fd, hwaddr addr, uint32_t size, uint32_t data, bool datamatch,
                          bool assign) {
    int ret;
    struct gh_fn_ioeventfd_arg io;
    struct gh_fn_desc fdesc;
    io.fd = fd;
    io.datamatch = datamatch ? data : 0;
    io.len = size;
    io.addr = addr;
    io.flags = datamatch ? GH_IOEVENTFD_FLAGS_DATAMATCH : 0;
    fdesc.type = GH_FN_IOEVENTFD;
    fdesc.arg_size = sizeof(struct gh_fn_ioeventfd_arg);
    fdesc.arg = (__u64)(&io);
    if (assign) {
        ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
    } else {
        ret = gunyah_vm_ioctl(GH_VM_REMOVE_FUNCTION, &fdesc);
    }
    return ret;
}
static void
gunyah_mem_ioeventfd_add(MemoryListener *listener, MemoryRegionSection *section, bool match_data,
                         uint64_t data, EventNotifier *e) {
    int fd = event_notifier_get_fd(e);
    int r;
    error_report("gh    │ioeventfd_add addr=0x%"
    PRIx64
    " size=0x%"
    PRIx64
    " fd=%d match=%d data=0x%"
    PRIx64, (uint64_t) section->offset_within_address_space, (uint64_t) int128_get64(
            section->size), fd, match_data, data);
    r = gunyah_set_ioeventfd_mmio(fd, section->offset_within_address_space,
                                  int128_get64(section->size), data, match_data, true);
    if (r < 0) {
        error_report("gh    │ioeventfd_add failed addr=0x%"
        PRIx64
        ": %s (errno=%d) "
        "— falling back to MMIO exit path (slower but functional)", (uint64_t) section->offset_within_address_space, strerror(
                errno), errno);
    }
}
static void
gunyah_mem_ioeventfd_del(MemoryListener *listener, MemoryRegionSection *section, bool match_data,
                         uint64_t data, EventNotifier *e) {
    int fd = event_notifier_get_fd(e);
    int r;
    r = gunyah_set_ioeventfd_mmio(fd, section->offset_within_address_space,
                                  int128_get64(section->size), data, match_data, false);
    if (r < 0) {
    }
}
