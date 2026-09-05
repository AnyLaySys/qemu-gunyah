#include "qemu/osdep.h"
#include "system/gunyah_report.h"
#undef gh_report
static const char *gh_color_for(const char *msg) {
  if (strstr(msg, "failed") || strstr(msg, "Error") ||
      strstr(msg, "crashed") || strstr(msg, "WDT") ||
      strstr(msg, "permanent") || strstr(msg, "stuck") ||
      strstr(msg, "invalid") || strstr(msg, "EBUSY") ||
      strstr(msg, "Warning")) {
    return gh_red;
  }
  if (strstr(msg, " OK") || strstr(msg, "Opened") ||
      strstr(msg, "created") || strstr(msg, "Installed") ||
      strstr(msg, "Loaded")) {
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
