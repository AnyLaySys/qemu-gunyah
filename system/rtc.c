
#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "system/system.h"
#include "system/rtc.h"

static enum {
    RTC_BASE_UTC,
    RTC_BASE_LOCALTIME,
    RTC_BASE_DATETIME,
} rtc_base_type = RTC_BASE_UTC;
static time_t rtc_ref_start_datetime;
static int rtc_realtime_clock_offset; /* used only with QEMU_CLOCK_REALTIME */
static int rtc_host_datetime_offset = -1; /* valid & used only with
                                             RTC_BASE_DATETIME */
QEMUClockType rtc_clock;
static time_t qemu_ref_timedate(QEMUClockType clock)
{
    time_t value = qemu_clock_get_ms(clock) / 1000;
    switch (clock) {
    case QEMU_CLOCK_REALTIME:
        value -= rtc_realtime_clock_offset;
    case QEMU_CLOCK_VIRTUAL:
        value += rtc_ref_start_datetime;
        break;
    case QEMU_CLOCK_HOST:
        if (rtc_base_type == RTC_BASE_DATETIME) {
            value -= rtc_host_datetime_offset;
        }
        break;
    default:
        g_assert_not_reached();
    }
    return value;
}

void qemu_get_timedate(struct tm *tm, time_t offset)
{
    time_t ti = qemu_ref_timedate(rtc_clock);

    ti += offset;

    switch (rtc_base_type) {
    case RTC_BASE_DATETIME:
    case RTC_BASE_UTC:
        gmtime_r(&ti, tm);
        break;
    case RTC_BASE_LOCALTIME:
        localtime_r(&ti, tm);
        break;
    }
}

time_t qemu_timedate_diff(struct tm *tm)
{
    time_t seconds;

    switch (rtc_base_type) {
    case RTC_BASE_DATETIME:
    case RTC_BASE_UTC:
        seconds = mktimegm(tm);
        break;
    case RTC_BASE_LOCALTIME:
    {
        struct tm tmp = *tm;
        tmp.tm_isdst = -1; /* use timezone to figure it out */
        seconds = mktime(&tmp);
        break;
    }
    default:
        abort();
    }

    return seconds - qemu_ref_timedate(QEMU_CLOCK_HOST);
}

static void configure_rtc_base_datetime(const char *startdate)
{
    time_t rtc_start_datetime;
    struct tm tm;

    if (sscanf(startdate, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon,
               &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
    } else if (sscanf(startdate, "%d-%d-%d",
                      &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
    } else {
        goto date_fail;
    }
    tm.tm_year -= 1900;
    tm.tm_mon--;
    rtc_start_datetime = mktimegm(&tm);
    if (rtc_start_datetime == -1) {
    date_fail:
        error_report("invalid datetime format");
        error_printf("valid formats: "
                     "'2006-06-17T16:01:21' or '2006-06-17'\n");
        exit(1);
    }
    rtc_host_datetime_offset = rtc_ref_start_datetime - rtc_start_datetime;
    rtc_ref_start_datetime = rtc_start_datetime;
}

void configure_rtc(QemuOpts *opts)
{
    const char *value;

    rtc_clock = QEMU_CLOCK_HOST;
    rtc_ref_start_datetime = qemu_clock_get_ms(QEMU_CLOCK_HOST) / 1000;
    rtc_realtime_clock_offset = qemu_clock_get_ms(QEMU_CLOCK_REALTIME) / 1000;

    value = qemu_opt_get(opts, "base");
    if (value) {
        if (!strcmp(value, "utc")) {
            rtc_base_type = RTC_BASE_UTC;
        } else if (!strcmp(value, "localtime")) {
            rtc_base_type = RTC_BASE_LOCALTIME;
        } else {
            rtc_base_type = RTC_BASE_DATETIME;
            configure_rtc_base_datetime(value);
        }
    }
    value = qemu_opt_get(opts, "clock");
    if (value) {
        if (!strcmp(value, "host")) {
            rtc_clock = QEMU_CLOCK_HOST;
        } else if (!strcmp(value, "rt")) {
            rtc_clock = QEMU_CLOCK_REALTIME;
        } else if (!strcmp(value, "vm")) {
            rtc_clock = QEMU_CLOCK_VIRTUAL;
        } else {
            error_report("invalid option value '%s'", value);
            exit(1);
        }
    }
    value = qemu_opt_get(opts, "driftfix");
    if (value) {
        if (!strcmp(value, "slew")) {
            warn_report("driftfix 'slew' is not available with this machine");
        } else if (!strcmp(value, "none")) {
        } else {
            error_report("invalid option value '%s'", value);
            exit(1);
        }
    }
}
