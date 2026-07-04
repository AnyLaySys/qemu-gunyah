#ifndef QEMU_XATTR_H
#define QEMU_XATTR_H



#ifdef CONFIG_LIBATTR
#  include <attr/xattr.h>
#else
#  if !defined(ENOATTR)
#    define ENOATTR ENODATA
#  endif
#  ifndef CONFIG_WIN32
#    include <sys/xattr.h>
#  endif
#endif

#endif
