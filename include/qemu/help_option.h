#ifndef QEMU_HELP_OPTION_H
#define QEMU_HELP_OPTION_H

static inline bool is_help_option(const char *s)
{
    return !strcmp(s, "?") || !strcmp(s, "help");
}

static inline int starts_with_help_option(const char *s)
{
    if (*s == '?') {
        return 1;
    }
    if (g_str_has_prefix(s, "help")) {
        return 4;
    }
    return 0;
}

#endif
