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
  if (addr && dladdr((void *)addr, &info) && info.dli_sname) {
    fprintf(stderr, "%s: 0x%llx = %s + 0x%lx [%s + 0x%lx]\n", label,
            (unsigned long long)addr, info.dli_sname,
            (unsigned long)(addr - (uintptr_t)info.dli_saddr),
            info.dli_fname ? info.dli_fname : "?",
            (unsigned long)(addr - (uintptr_t)info.dli_fbase));
  } else if (addr && dladdr((void *)addr, &info)) {
    fprintf(stderr, "%s: 0x%llx = %s + 0x%lx\n", label,
            (unsigned long long)addr, info.dli_fname ? info.dli_fname : "?",
            (unsigned long)(addr - (uintptr_t)info.dli_fbase));
  } else {
    fprintf(stderr, "%s: 0x%llx (no symbol info)\n", label,
            (unsigned long long)addr);
  }
}
#define SIGBUS_MAX_LEND_REGIONS GUNYAH_MAX_MEM_SLOTS
static struct {
  uint8_t *hva;
  uint64_t size;
  uint64_t gpa;
} sigbus_lend_regions[SIGBUS_MAX_LEND_REGIONS];
static volatile int sigbus_lend_count;
static volatile int sigbus_handler_active;
static struct sigaction sigbus_previous_action;
static bool sigbus_previous_valid;
static bool sigbus_chain_mode;
static pthread_once_t sigbus_install_once = PTHREAD_ONCE_INIT;
static int sigbus_install_errno;
static int sigbus_install_result;
static int sigsegv_install_result;
static bool gunyah_sigbus_is_lend(const void *addr) {
  uintptr_t fault = (uintptr_t)addr;
  int count = sigbus_lend_count;
  int i;
  for (i = 0; i < count; i++) {
    uintptr_t start = (uintptr_t)sigbus_lend_regions[i].hva;
    if (fault >= start && fault - start < sigbus_lend_regions[i].size) {
      return true;
    }
  }
  return false;
}
static void gunyah_signal_reset(void) {
  sigbus_handler_active = 0;
  __sync_synchronize();
  sigbus_lend_count = 0;
}
static void gunyah_signal_register_lend(uint8_t *hva, uint64_t size,
                                        uint64_t gpa) {
  int index = sigbus_lend_count;
  if (index >= SIGBUS_MAX_LEND_REGIONS) {
    error_report("Too many Gunyah LEND regions");
    exit(1);
  }
  sigbus_lend_regions[index].hva = hva;
  sigbus_lend_regions[index].size = size;
  sigbus_lend_regions[index].gpa = gpa;
  __sync_synchronize();
  sigbus_lend_count = index + 1;
}
static void gunyah_chain_signal(int sig, siginfo_t *si, void *ctx) {
  if (!sigbus_previous_valid ||
      sigbus_previous_action.sa_handler == SIG_DFL) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }
  if (sigbus_previous_action.sa_handler == SIG_IGN) {
    return;
  }
  if (sigbus_previous_action.sa_flags & SA_SIGINFO) {
    sigbus_previous_action.sa_sigaction(sig, si, ctx);
  } else {
    sigbus_previous_action.sa_handler(sig);
  }
}
static void gunyah_sigsegv_handler(int sig, siginfo_t *si, void *ctx) {
  ucontext_t *uc = (ucontext_t *)ctx;
  int i;
  if (sig == SIGBUS && si->si_code == BUS_ADRERR && si->si_addr &&
      sigbus_handler_active &&
      gunyah_sigbus_is_lend(si->si_addr)) {
    uintptr_t page_addr = (uintptr_t)si->si_addr & ~(uintptr_t)0xFFF;
    void *ret = mmap((void *)page_addr, 4096, PROT_READ | PROT_WRITE,
                     MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret != MAP_FAILED) {
      static volatile int remap_log_count;
      if (remap_log_count < 50) {
        char buf[128];
        int len = snprintf(buf, sizeof(buf),
                           "gh SIGBUS at hva=0x%llx code=%d — remapped\n",
                           (unsigned long long)page_addr, si->si_code);
        if (len > 0)
          write(STDERR_FILENO, buf, len);
        remap_log_count++;
      }
      return;
    }
  }
  if (sig == SIGBUS && sigbus_chain_mode) {
    gunyah_chain_signal(sig, si, ctx);
    return;
  }
  {
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "\n=== GUNYAH SIGNAL DIAGNOSTIC ===\n"
                       "Signal: %d (%s)\n"
                       "Faulting address: %p\n"
                       "si_code: %d\n"
                       "handler_active: %d, lend_count: %d\n"
                       "Thread ID: %d\n",
                       sig,
                       sig == SIGSEGV  ? "SIGSEGV"
                       : sig == SIGBUS ? "SIGBUS"
                                       : "?",
                       si->si_addr, si->si_code, sigbus_handler_active,
                       sigbus_lend_count, (int)syscall(SYS_gettid));
    if (len > 0)
      write(STDERR_FILENO, buf, len);
  }
  if (uc) {
    char buf[128];
    uintptr_t pc = (uintptr_t)uc->uc_mcontext.pc;
    uintptr_t lr = (uintptr_t)uc->uc_mcontext.regs[30];
    int len = snprintf(buf, sizeof(buf), "PC=0x%llx LR=0x%llx SP=0x%llx\n",
                       (unsigned long long)pc, (unsigned long long)lr,
                       (unsigned long long)uc->uc_mcontext.sp);
    if (len > 0)
      write(STDERR_FILENO, buf, len);
    gunyah_print_symbol("PC", pc);
    gunyah_print_symbol("LR", lr);
  }
  for (i = 0; i < sigbus_lend_count; i++) {
    char buf[128];
    uint8_t *fault = (uint8_t *)si->si_addr;
    uint8_t *start = sigbus_lend_regions[i].hva;
    uint8_t *end = start + sigbus_lend_regions[i].size;
    int len =
        snprintf(buf, sizeof(buf), "  lend[%d]: hva=%p-%p gpa=0x%llx%s\n", i,
                 start, end, (unsigned long long)sigbus_lend_regions[i].gpa,
                 (fault >= start && fault < end) ? " *** FAULT ***" : "");
    if (len > 0)
      write(STDERR_FILENO, buf, len);
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
  long ret = syscall(SYS_rt_sigaction, sig, &ksa, NULL, (size_t)8);
  return (int)ret;
}
static int chained_sigaction(int sig,
                             void (*handler)(int, siginfo_t *, void *)) {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_sigaction = handler;
  action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
  return sigaction(sig, &action, &sigbus_previous_action);
}
static void gunyah_install_sigsegv_handler_once(void) {
  sigbus_chain_mode = getenv("GUNYAH_CHAIN_SIGNALS") != NULL;
  if (getenv("GUNYAH_NO_SIGSEGV_HANDLER")) {
    sigsegv_install_result = 0;
  } else {
    sigsegv_install_result =
        raw_sigaction(SIGSEGV, gunyah_sigsegv_handler);
  }
  sigbus_install_result = sigbus_chain_mode ?
      chained_sigaction(SIGBUS, gunyah_sigsegv_handler) :
      raw_sigaction(SIGBUS, gunyah_sigsegv_handler);
  sigbus_install_errno = sigbus_install_result ? errno : 0;
  sigbus_previous_valid =
      sigbus_chain_mode && sigbus_install_result == 0;
  gh_report("installed SIGSEGV/SIGBUS handler via %s "
            "(SIGSEGV=%d SIGBUS=%d)",
            sigbus_chain_mode ? "sigaction chain" : "raw rt_sigaction",
            sigsegv_install_result, sigbus_install_result);
}
static void gunyah_install_sigsegv_handler(void) {
  sigset_t set;
  int ret = pthread_once(&sigbus_install_once,
                         gunyah_install_sigsegv_handler_once);
  if (ret || sigbus_install_result) {
    error_report("Could not install Gunyah SIGBUS handler: %s",
                 strerror(ret ? ret : sigbus_install_errno));
    exit(1);
  }
  sigbus_handler_active = 1;
  __sync_synchronize();
  sigemptyset(&set);
  sigaddset(&set, SIGBUS);
  sigaddset(&set, SIGSEGV);
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
}
