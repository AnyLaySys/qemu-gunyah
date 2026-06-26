
#include "qapi/qapi-types-common.h"

HumanReadableText *human_readable_text_from_str(GString *str);

char **strv_from_str_list(const strList *list);
