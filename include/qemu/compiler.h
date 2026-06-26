
#ifndef COMPILER_H
#define COMPILER_H

#define HOST_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

#define HOST_LONG_BITS (__SIZEOF_POINTER__ * 8)

#if defined __clang_analyzer__ || defined __COVERITY__
#define QEMU_STATIC_ANALYSIS 1
#endif

#ifdef __cplusplus
#define QEMU_EXTERN_C extern "C"
#else
#define QEMU_EXTERN_C extern
#endif

#define QEMU_PACKED __attribute__((packed))
#define QEMU_ALIGNED(X) __attribute__((aligned(X)))

#ifndef glue
#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#define stringify(s) tostring(s)
#define tostring(s) #s
#endif

#define MAKE_IDENTIFIER(stem) glue(stem, __COUNTER__)

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof(((type *) 0)->member) *__mptr = (ptr);     \
        (type *) ((char *) __mptr - offsetof(type, member));})
#endif

#define sizeof_field(type, field) sizeof(((type *)0)->field)

#define endof(container, field) \
    (offsetof(container, field) + sizeof_field(container, field))

#define DO_UPCAST(type, field, dev) ( __extension__ ( { \
    char __attribute__((unused)) offset_must_be_zero[ \
        -offsetof(type, field)]; \
    container_of(dev, type, field);}))

#define typeof_field(type, field) typeof(((type *)0)->field)
#define type_check(t1,t2) ((t1*)0 - (t2*)0)

#define QEMU_BUILD_BUG_ON_STRUCT(x) \
    struct { \
        int:(x) ? -1 : 1; \
    }

#define QEMU_BUILD_BUG_MSG(x, msg) _Static_assert(!(x), msg)

#define QEMU_BUILD_BUG_ON(x) QEMU_BUILD_BUG_MSG(x, "not expecting: " #x)

#define QEMU_BUILD_BUG_ON_ZERO(x) (sizeof(QEMU_BUILD_BUG_ON_STRUCT(x)) - \
                                   sizeof(QEMU_BUILD_BUG_ON_STRUCT(x)))

#if !defined(__clang__) && defined(_WIN32)
# define __printf__ __gnu_printf__
#endif

#ifndef __has_warning
#define __has_warning(x) 0 /* compatibility with non-clang compilers */
#endif

#ifndef __has_feature
#define __has_feature(x) 0 /* compatibility with non-clang compilers */
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0 /* compatibility with non-clang compilers */
#endif

#if __has_builtin(__builtin_assume_aligned) || !defined(__clang__)
#define HAS_ASSUME_ALIGNED
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0 /* compatibility with older GCC */
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
# define QEMU_SANITIZE_ADDRESS 1
#endif

#if defined(__SANITIZE_THREAD__) || __has_feature(thread_sanitizer)
# define QEMU_SANITIZE_THREAD 1
#endif

#if __has_attribute(flatten) || !defined(__clang__)
# define QEMU_FLATTEN __attribute__((flatten))
#else
# define QEMU_FLATTEN
#endif

#if __has_attribute(error)
# define QEMU_ERROR(X) __attribute__((error(X)))
#else
# define QEMU_ERROR(X)
#endif

#if __has_attribute(nonstring)
# define QEMU_NONSTRING __attribute__((nonstring))
#else
# define QEMU_NONSTRING
#endif

#if defined(__OPTIMIZE__)
#define QEMU_ALWAYS_INLINE  __attribute__((always_inline))
#else
#define QEMU_ALWAYS_INLINE
#endif

#if __has_attribute(fallthrough)
# define QEMU_FALLTHROUGH __attribute__((fallthrough))
#else
# define QEMU_FALLTHROUGH do {} while (0) /* fallthrough */
#endif

#ifdef CONFIG_CFI
#define QEMU_DISABLE_CFI __attribute__((no_sanitize("cfi-icall")))
#else
#define QEMU_DISABLE_CFI
#endif

#if defined(__apple_build_version__) && __clang_major__ >= 14
#define BUILTIN_SUBCLL_BROKEN
#endif

#if __has_attribute(annotate)
#define QEMU_ANNOTATE(x) __attribute__((annotate(x)))
#else
#define QEMU_ANNOTATE(x)
#endif

#if __has_attribute(used)
# define QEMU_USED __attribute__((used))
#else
# define QEMU_USED
#endif

#ifdef __clang__
#  define TSA(x)   __attribute__((x))
#else
#  define TSA(x)   /* No TSA, make TSA attributes no-ops. */
#endif

#define TSA_CAPABILITY(x) TSA(capability(x))

#define TSA_GUARDED_BY(x) TSA(guarded_by(x))

#define TSA_PT_GUARDED_BY(x) TSA(pt_guarded_by(x))

#define TSA_REQUIRES(...) TSA(requires_capability(__VA_ARGS__))
#define TSA_REQUIRES_SHARED(...) TSA(requires_shared_capability(__VA_ARGS__))

#define TSA_EXCLUDES(...) TSA(locks_excluded(__VA_ARGS__))

#define TSA_ACQUIRE(...) TSA(acquire_capability(__VA_ARGS__))
#define TSA_ACQUIRE_SHARED(...) TSA(acquire_shared_capability(__VA_ARGS__))

#define TSA_RELEASE(...) TSA(release_capability(__VA_ARGS__))
#define TSA_RELEASE_SHARED(...) TSA(release_shared_capability(__VA_ARGS__))

#define TSA_NO_TSA TSA(no_thread_safety_analysis)

#define TSA_ASSERT(...) TSA(assert_capability(__VA_ARGS__))
#define TSA_ASSERT_SHARED(...) TSA(assert_shared_capability(__VA_ARGS__))

#define IS_ENABLED(x)                  IS_EMPTY(x)

#define IS_EMPTY_JUNK_                 junk,
#define IS_EMPTY(value)                IS_EMPTY_(IS_EMPTY_JUNK_##value)

#define SECOND_ARG(first, second, ...) second
#define IS_EMPTY_(junk_maybecomma)     SECOND_ARG(junk_maybecomma 1, 0)

#ifndef __cplusplus
#define typeof_strip_qual(expr)                                                    \
  typeof(                                                                          \
    __builtin_choose_expr(                                                         \
      __builtin_types_compatible_p(typeof(expr), bool) ||                          \
        __builtin_types_compatible_p(typeof(expr), const bool) ||                  \
        __builtin_types_compatible_p(typeof(expr), volatile bool) ||               \
        __builtin_types_compatible_p(typeof(expr), const volatile bool),           \
        (bool)1,                                                                   \
    __builtin_choose_expr(                                                         \
      __builtin_types_compatible_p(typeof(expr), signed char) ||                   \
        __builtin_types_compatible_p(typeof(expr), const signed char) ||           \
        __builtin_types_compatible_p(typeof(expr), volatile signed char) ||        \
        __builtin_types_compatible_p(typeof(expr), const volatile signed char),    \
        (signed char)1,                                                            \
    __builtin_choose_expr(                                                         \
      __builtin_types_compatible_p(typeof(expr), unsigned char) ||                 \
        __builtin_types_compatible_p(typeof(expr), const unsigned char) ||         \
        __builtin_types_compatible_p(typeof(expr), volatile unsigned char) ||      \
        __builtin_types_compatible_p(typeof(expr), const volatile unsigned char),  \
        (unsigned char)1,                                                          \
    __builtin_choose_expr(                                                         \
      __builtin_types_compatible_p(typeof(expr), signed short) ||                  \
        __builtin_types_compatible_p(typeof(expr), const signed short) ||          \
        __builtin_types_compatible_p(typeof(expr), volatile signed short) ||       \
        __builtin_types_compatible_p(typeof(expr), const volatile signed short),   \
        (signed short)1,                                                           \
    __builtin_choose_expr(                                                         \
      __builtin_types_compatible_p(typeof(expr), unsigned short) ||                \
        __builtin_types_compatible_p(typeof(expr), const unsigned short) ||        \
        __builtin_types_compatible_p(typeof(expr), volatile unsigned short) ||     \
        __builtin_types_compatible_p(typeof(expr), const volatile unsigned short), \
        (unsigned short)1,                                                         \
      (expr)+0))))))
#endif

#endif /* COMPILER_H */
