// test_rust_bt.c
#include <stdio.h>
#include <stdint.h>
#include "test_rust_bt.h"

static int on_dump(void *ctx, uintptr_t pc, const char *filename, int lineno,
                   const char *function, void *data) {
  printf("pc=%p, file=%s:%d, func=%s\n",
         (void *)pc, filename ? filename : "<null>", lineno,
         function ? function : "<null>");
  // 返回非 0 可中断遍历，例如：捕获前 10 帧后中断
  return 0;
}

static void on_error(void *ctx, const char *msg, int errnum, void *data) {
  fprintf(stderr, "on_error err=%d msg=%s\n", errnum, msg ? msg : "<null>");
}

int main() {
  // skip 传 0（内部已额外隐藏桥接栈帧）
  int rc = ten_rust_backtrace_dump(NULL, on_dump, on_error, 0);
  printf("ten_rust_backtrace_dump rc=%d\n", rc);
  return 0;
}