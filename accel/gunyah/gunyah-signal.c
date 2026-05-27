__attribute__((constructor))
static void gunyah_unblock_sigbus_early(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGBUS);
    sigaddset(&set, SIGSEGV);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}
enum gh_vm_exit_type {
    GH_RM_EXIT_TYPE_VM_EXIT = 0,
    GH_RM_EXIT_TYPE_PSCI_POWER_OFF = 1,
    GH_RM_EXIT_TYPE_PSCI_SYSTEM_RESET = 2,
    GH_RM_EXIT_TYPE_PSCI_SYSTEM_RESET2 = 3,
    GH_RM_EXIT_TYPE_WDT_BITE = 4,
    GH_RM_EXIT_TYPE_HYP_ERROR = 5,
    GH_RM_EXIT_TYPE_ASYNC_EXT_ABORT = 6,
    GH_RM_EXIT_TYPE_VM_FORCE_STOPPED = 7,
};
static void gunyah_print_symbol(const char *label, uintptr_t addr) {
    Dl_info info;
    if (addr && dladdr((void *) addr, &info) && info.dli_sname) {
        fprintf(stderr, "%s: 0x%llx = %s + 0x%lx [%s]\n", label, (unsigned long long) addr,
                info.dli_sname, (unsigned long) (addr - (uintptr_t) info.dli_saddr),
                info.dli_fname ? info.dli_fname : "?");
    } else if (addr && dladdr((void *) addr, &info)) {
        fprintf(stderr, "%s: 0x%llx = ??? (in %s, nearest: %s)\n", label, (unsigned long long) addr,
                info.dli_fname ? info.dli_fname : "?", info.dli_sname ? info.dli_sname : "unknown");
    } else {
        fprintf(stderr, "%s: 0x%llx (no symbol info)\n", label, (unsigned long long) addr);
    }
}
#define SIGBUS_MAX_LEND_REGIONS 64
static struct {
    uint8_t *hva;
    uint64_t size;
    uint64_t gpa;
} sigbus_lend_regions[SIGBUS_MAX_LEND_REGIONS];
static volatile int sigbus_lend_count;
static volatile int sigbus_handler_active;
static void gunyah_sigsegv_handler(int sig, siginfo_t *si, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;
    int i;
    if (sig == SIGBUS && si->si_addr && sigbus_handler_active) {
        uintptr_t page_addr = (uintptr_t) si->si_addr & ~(uintptr_t) 0xFFF;
        void *ret = mmap((void *) page_addr, 4096, PROT_READ | PROT_WRITE,
                         MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ret != MAP_FAILED) {
            static volatile int remap_log_count;
            if (remap_log_count < 50) {
                char buf[128];
                int len = snprintf(buf, sizeof(buf),
                                   "gh SIGBUS at hva=0x%llx code=%d — remapped\n",
                                   (unsigned long long) page_addr, si->si_code);
                if (len > 0) write(STDERR_FILENO, buf, len);
                remap_log_count++;
            }
            return;
        }
    }
    {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "\n=== GUNYAH SIGNAL DIAGNOSTIC ===\n"
                                             "Signal: %d (%s)\n"
                                             "Faulting address: %p\n"
                                             "si_code: %d\n"
                                             "handler_active: %d, lend_count: %d\n"
                                             "Thread ID: %d\n", sig,
                           sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : "?", si->si_addr,
                           si->si_code, sigbus_handler_active, sigbus_lend_count,
                           (int) syscall(SYS_gettid));
        if (len > 0) write(STDERR_FILENO, buf, len);
    }
    if (uc) {
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "PC=0x%llx LR=0x%llx SP=0x%llx\n",
                           (unsigned long long) uc->uc_mcontext.pc,
                           (unsigned long long) uc->uc_mcontext.regs[30],
                           (unsigned long long) uc->uc_mcontext.sp);
        if (len > 0) write(STDERR_FILENO, buf, len);
    }
    for (i = 0; i < sigbus_lend_count; i++) {
        char buf[128];
        uint8_t *fault = (uint8_t *) si->si_addr;
        uint8_t *start = sigbus_lend_regions[i].hva;
        uint8_t *end = start + sigbus_lend_regions[i].size;
        int len = snprintf(buf, sizeof(buf), "  lend[%d]: hva=%p-%p gpa=0x%llx%s\n", i, start, end,
                           (unsigned long long) sigbus_lend_regions[i].gpa,
                           (fault >= start && fault < end) ? " *** FAULT ***" : "");
        if (len > 0) write(STDERR_FILENO, buf, len);
    }
    {
        const char msg[] = "=== END DIAGNOSTIC ===\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}
struct kernel_sigaction_arm64 {
    void (*k_sa_handler)(int, siginfo_t *, void *);
    unsigned long sa_flags;
    unsigned long sa_mask;
};
static int raw_sigaction(int sig, void (*handler)(int, siginfo_t *, void *)) {
    struct kernel_sigaction_arm64 ksa;
    memset(&ksa, 0, sizeof(ksa));
    ksa.k_sa_handler = handler;
    ksa.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
    long ret = syscall(SYS_rt_sigaction, sig, &ksa, NULL, (size_t) 8);
    return (int) ret;
}
static void gunyah_install_sigsegv_handler(void) {
    sigset_t set;
    int r1, r2;
    r1 = raw_sigaction(SIGSEGV, gunyah_sigsegv_handler);
    r2 = raw_sigaction(SIGBUS, gunyah_sigsegv_handler);
    sigbus_handler_active = 1;
    sigemptyset(&set);
    sigaddset(&set, SIGBUS);
    sigaddset(&set, SIGSEGV);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    error_report(gh"installed SIGSEGV/SIGBUS handler via raw rt_sigaction "
                 "(SIGSEGV=%d SIGBUS=%d)", r1, r2);
}
