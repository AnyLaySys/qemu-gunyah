
#include "qemu/osdep.h"

#include "qemu-version.h"
#include "qapi/qapi-commands-control.h"

VersionInfo *qmp_query_version(Error **errp)
{
    VersionInfo *info = g_new0(VersionInfo, 1);

    info->qemu = g_new0(VersionTriple, 1);
    info->qemu->major = QEMU_VERSION_MAJOR;
    info->qemu->minor = QEMU_VERSION_MINOR;
    info->qemu->micro = QEMU_VERSION_MICRO;
    info->package = g_strdup(QEMU_PKGVERSION);

    return info;
}
