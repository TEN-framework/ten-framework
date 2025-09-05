      // rust_bt_ffi.h
      #include <stdint.h>
      #ifdef __cplusplus
      extern "C" {
      #endif
      int ten_rust_backtrace_dump(void *ctx,
                                  int (*on_dump)(void *ctx, uintptr_t pc,
                                                 const char *filename, int lineno,
                                                 const char *function, void *data),
                                  void (*on_error)(void *ctx, const char *msg, int errnum, void *data),
                                  uintptr_t skip);
      #ifdef __cplusplus
      }
      #endif