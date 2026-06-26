
#ifndef SYSTEM_RTC_H
#define SYSTEM_RTC_H

void qemu_get_timedate(struct tm *tm, time_t offset);

time_t qemu_timedate_diff(struct tm *tm);

#endif
