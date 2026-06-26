

#ifndef ERROR_H
#define ERROR_H

#include "qapi/qapi-types-error.h"

typedef enum ErrorClass {
    ERROR_CLASS_GENERIC_ERROR = QAPI_ERROR_CLASS_GENERICERROR,
    ERROR_CLASS_COMMAND_NOT_FOUND = QAPI_ERROR_CLASS_COMMANDNOTFOUND,
    ERROR_CLASS_DEVICE_NOT_ACTIVE = QAPI_ERROR_CLASS_DEVICENOTACTIVE,
    ERROR_CLASS_DEVICE_NOT_FOUND = QAPI_ERROR_CLASS_DEVICENOTFOUND,
    ERROR_CLASS_KVM_MISSING_CAP = QAPI_ERROR_CLASS_KVMMISSINGCAP,
} ErrorClass;

const char *error_get_pretty(const Error *err);

ErrorClass error_get_class(const Error *err);

#define error_setg(errp, fmt, ...)                              \
    error_setg_internal((errp), __FILE__, __LINE__, __func__,   \
                        (fmt), ## __VA_ARGS__)
void error_setg_internal(Error **errp,
                         const char *src, int line, const char *func,
                         const char *fmt, ...)
    G_GNUC_PRINTF(5, 6);

#define error_setg_errno(errp, os_error, fmt, ...)                      \
    error_setg_errno_internal((errp), __FILE__, __LINE__, __func__,     \
                              (os_error), (fmt), ## __VA_ARGS__)
void error_setg_errno_internal(Error **errp,
                               const char *fname, int line, const char *func,
                               int os_error, const char *fmt, ...)
    G_GNUC_PRINTF(6, 7);

#ifdef _WIN32
#define error_setg_win32(errp, win32_err, fmt, ...)                     \
    error_setg_win32_internal((errp), __FILE__, __LINE__, __func__,     \
                              (win32_err), (fmt), ## __VA_ARGS__)
void error_setg_win32_internal(Error **errp,
                               const char *src, int line, const char *func,
                               int win32_err, const char *fmt, ...)
    G_GNUC_PRINTF(6, 7);
#endif

void error_propagate(Error **dst_errp, Error *local_err);


void error_propagate_prepend(Error **dst_errp, Error *local_err,
                             const char *fmt, ...)
    G_GNUC_PRINTF(3, 4);

void error_vprepend(Error *const *errp, const char *fmt, va_list ap)
    G_GNUC_PRINTF(2, 0);

void error_prepend(Error *const *errp, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

void error_append_hint(Error *const *errp, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

#define error_setg_file_open(errp, os_errno, filename)                  \
    error_setg_file_open_internal((errp), __FILE__, __LINE__, __func__, \
                                  (os_errno), (filename))
void error_setg_file_open_internal(Error **errp,
                                   const char *src, int line, const char *func,
                                   int os_errno, const char *filename);

Error *error_copy(const Error *err);

void error_free(Error *err);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Error, error_free)

void error_free_or_abort(Error **errp);

void warn_report_err(Error *err);

void error_report_err(Error *err);

void warn_reportf_err(Error *err, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

void error_reportf_err(Error *err, const char *fmt, ...)
    G_GNUC_PRINTF(2, 3);

bool warn_report_err_once_cond(bool *printed, Error *err);

#define warn_report_err_once(err)                           \
    ({                                                      \
        static bool print_once_;                            \
        warn_report_err_once_cond(&print_once_, err);       \
    })

#define error_set(errp, err_class, fmt, ...)                    \
    error_set_internal((errp), __FILE__, __LINE__, __func__,    \
                       (err_class), (fmt), ## __VA_ARGS__)
void error_set_internal(Error **errp,
                        const char *src, int line, const char *func,
                        ErrorClass err_class, const char *fmt, ...)
    G_GNUC_PRINTF(6, 7);

#define ERRP_GUARD()                                            \
    g_auto(ErrorPropagator) _auto_errp_prop = {.errp = errp};   \
    do {                                                        \
        if (!errp || errp == &error_fatal) {                    \
            errp = &_auto_errp_prop.local_err;                  \
        }                                                       \
    } while (0)

typedef struct ErrorPropagator {
    Error *local_err;
    Error **errp;
} ErrorPropagator;

static inline void error_propagator_cleanup(ErrorPropagator *prop)
{
    error_propagate(prop->errp, prop->local_err);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(ErrorPropagator, error_propagator_cleanup);

extern Error *error_warn;

extern Error *error_abort;

extern Error *error_fatal;

#endif
