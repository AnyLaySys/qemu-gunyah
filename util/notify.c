
#include "qemu/osdep.h"
#include "qemu/notify.h"

void notifier_list_init(NotifierList *list)
{
    QLIST_INIT(&list->notifiers);
}

void notifier_list_add(NotifierList *list, Notifier *notifier)
{
    QLIST_INSERT_HEAD(&list->notifiers, notifier, node);
}

void notifier_remove(Notifier *notifier)
{
    QLIST_REMOVE(notifier, node);
}

void notifier_list_notify(NotifierList *list, void *data)
{
    Notifier *notifier, *next;

    QLIST_FOREACH_SAFE(notifier, &list->notifiers, node, next) {
        notifier->notify(notifier, data);
    }
}

bool notifier_list_empty(NotifierList *list)
{
    return QLIST_EMPTY(&list->notifiers);
}

void notifier_with_return_list_init(NotifierWithReturnList *list)
{
    QLIST_INIT(&list->notifiers);
}

void notifier_with_return_list_add(NotifierWithReturnList *list,
                                   NotifierWithReturn *notifier)
{
    QLIST_INSERT_HEAD(&list->notifiers, notifier, node);
}

void notifier_with_return_remove(NotifierWithReturn *notifier)
{
    QLIST_REMOVE(notifier, node);
}

int notifier_with_return_list_notify(NotifierWithReturnList *list, void *data,
                                     Error **errp)
{
    NotifierWithReturn *notifier, *next;
    int ret = 0;

    QLIST_FOREACH_SAFE(notifier, &list->notifiers, node, next) {
        ret = notifier->notify(notifier, data, errp);
        if (ret != 0) {
            break;
        }
    }
    return ret;
}
