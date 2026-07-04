#ifndef QEMU_QSP_H
#define QEMU_QSP_H

enum QSPSortBy {
    QSP_SORT_BY_TOTAL_WAIT_TIME,
    QSP_SORT_BY_AVG_WAIT_TIME,
};

void qsp_report(size_t max, enum QSPSortBy sort_by,
                bool callsite_coalesce);

bool qsp_is_enabled(void);
void qsp_enable(void);
void qsp_disable(void);
void qsp_reset(void);

#endif /* QEMU_QSP_H */
