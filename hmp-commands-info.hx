    {
        .name       = "version",
        .args_type  = "",
        .params     = "",
        .help       = "show the version of QEMU",
        .cmd        = hmp_info_version,
        .flags      = "p",
    },

    {
        .name       = "status",
        .args_type  = "",
        .params     = "",
        .help       = "show the current VM status",
        .cmd        = hmp_info_status,
        .flags      = "p",
    },

    {
        .name       = "name",
        .args_type  = "",
        .params     = "",
        .help       = "show the current VM name",
        .cmd        = hmp_info_name,
        .flags      = "p",
    },
