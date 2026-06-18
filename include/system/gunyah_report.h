#ifndef GUNYAH_REPORT_H
#define GUNYAH_REPORT_H

#define gh_red "\033[0;1;31m"
#define gh_yellow "\033[0;1;38:5:185m"
#define gh_green "\033[0;32m"
#define gh_highlight "\033[0;1;39m"
#define gh_normal "\033[0m"

void gh_report(const char *file, const char *func, int line,
               const char *fmt, ...);
#define gh_report(...) gh_report(__FILE__, __func__, __LINE__, __VA_ARGS__)

#endif
