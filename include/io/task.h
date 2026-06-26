
#ifndef QIO_TASK_H
#define QIO_TASK_H

typedef struct QIOTask QIOTask;

typedef void (*QIOTaskFunc)(QIOTask *task,
                            gpointer opaque);

typedef void (*QIOTaskWorker)(QIOTask *task,
                              gpointer opaque);


QIOTask *qio_task_new(Object *source,
                      QIOTaskFunc func,
                      gpointer opaque,
                      GDestroyNotify destroy);

void qio_task_run_in_thread(QIOTask *task,
                            QIOTaskWorker worker,
                            gpointer opaque,
                            GDestroyNotify destroy,
                            GMainContext *context);


void qio_task_wait_thread(QIOTask *task);


void qio_task_complete(QIOTask *task);


void qio_task_set_error(QIOTask *task,
                        Error *err);


bool qio_task_propagate_error(QIOTask *task,
                              Error **errp);


void qio_task_set_result_pointer(QIOTask *task,
                                 gpointer result,
                                 GDestroyNotify notify);


gpointer qio_task_get_result_pointer(QIOTask *task);


Object *qio_task_get_source(QIOTask *task);

#endif /* QIO_TASK_H */
