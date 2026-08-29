#include "qemu/osdep.h"
#include "system/gunyah_report.h"
#undef gh_report
static const char *gh_color_for(const char *msg) {
  uint64_t ok, skipped, failed;
  if (sscanf(msg,
             "%*s collapse: %" SCNu64 " OK, %" SCNu64 " skipped, %" SCNu64
             " failed",
             &ok, &skipped, &failed) == 3) {
    return failed == 0 && ok > 0 ? gh_green : gh_yellow;
  }
  if (strstr(msg, "FAILED") || strstr(msg, "failed") || strstr(msg, "ERROR") ||
      strstr(msg, "Error") || strstr(msg, "CRASHED") ||
      strstr(msg, "WDT BITE") || strstr(msg, "WILL SIGBUS") ||
      strstr(msg, "invalid") || strstr(msg, "timeout") ||
      strstr(msg, "Timed out") || strstr(msg, "Dependency") ||
      strstr(msg, "dependency") || strstr(msg, "Assertion") ||
      strstr(msg, "assert") || strstr(msg, "not supported") ||
      strstr(msg, "unsupported") || strstr(msg, "ENOTTY") ||
      strstr(msg, "started before") || strstr(msg, "cannot be started again") ||
      strstr(msg, "frozen") || strstr(msg, "concurrency") ||
      strstr(msg, "CONCUR") || strstr(msg, "WARNING") ||
      strstr(msg, "Warning") || strstr(msg, "warn")) {
    return gh_red;
  }
  if (strstr(msg, " OK") || strstr(msg, "OK") || strstr(msg, "opened") ||
      strstr(msg, "created") || strstr(msg, "loaded") ||
      strstr(msg, "placed") || strstr(msg, "registered") ||
      strstr(msg, "armed") || strstr(msg, "done") || strstr(msg, "started") ||
      strstr(msg, "enabled") || strstr(msg, "kept mapped")) {
    return gh_green;
  }
  return gh_yellow;
}
static const char *gh_file_name(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}
void gh_report(const char *file, const char *func, int line, const char *fmt,
               ...) {
  va_list ap;
  char msg[1024];
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  fprintf(stderr, " %s\xe2\x80\xa2%s %s %s:%s:%d\n", gh_color_for(msg),
          gh_normal, msg, gh_file_name(file), func, line);
}
