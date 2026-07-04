
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/type-helpers.h"

HumanReadableText *human_readable_text_from_str(GString *str)
{
    HumanReadableText *ret = g_new0(HumanReadableText, 1);

    ret->human_readable_text = g_steal_pointer(&str->str);

    return ret;
}

char **strv_from_str_list(const strList *list)
{
    const strList *tail;
    int i = 0;
    char **strv = g_new(char *, QAPI_LIST_LENGTH(list) + 1);

    for (tail = list; tail != NULL; tail = tail->next) {
        strv[i++] = g_strdup(tail->value);
    }
    strv[i] = NULL;

    return strv;
}
