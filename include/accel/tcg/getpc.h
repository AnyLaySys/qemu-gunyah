
#ifndef ACCEL_TCG_GETPC_H
#define ACCEL_TCG_GETPC_H

#ifndef CONFIG_TCG
#error Can only include this header with TCG
#endif

#ifdef CONFIG_TCG_INTERPRETER
# ifdef __ANDROID__
uintptr_t *android_tci_tb_ptr_ptr(void);
#  define tci_tb_ptr (*android_tci_tb_ptr_ptr())
# else
extern __thread uintptr_t tci_tb_ptr;
# endif
# define GETPC() tci_tb_ptr
#else
# define GETPC() \
    ((uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)))
#endif

#endif /* ACCEL_TCG_GETPC_H */
