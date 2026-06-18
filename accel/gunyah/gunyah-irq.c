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
    gh_report("add_irqfd label=%d fd=%d flags=0x%x", label, irqfd, ghirqfd.flags);
    ret = gunyah_vm_ioctl(GH_VM_ADD_FUNCTION, &fdesc);
    if (ret) {
        gh_report("add_irqfd FAILED label=%d: %s (errno=%d)", label, strerror(errno), errno);
        error_setg_errno(errp, errno, "GH_FN_IRQFD failed");
    } else {
        gh_report("add_irqfd OK label=%d", label);
    }
    return ret;
}
