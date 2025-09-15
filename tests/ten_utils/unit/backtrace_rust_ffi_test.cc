//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <gtest/gtest.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "include_internal/ten_rust/ten_rust.h"

namespace {

int g_frame_count = 0;

int on_dump(void *ctx, uintptr_t pc, const char *filename, int lineno,
            const char *function, void *data) {
  (void)ctx;
  (void)data;
  // record frame count and output one line, return 0 to continue
  (void)printf("pc=0x%0" PRIxPTR ", file=%s:%d, func=%s\n", pc,
               (filename != nullptr) ? filename : "<null>", lineno,
               (function != nullptr) ? function : "<null>");
  ++g_frame_count;
  return 0;
}

void on_error(void *ctx, const char *msg, int errnum, void *data) {
  (void)ctx;
  (void)data;
  (void)fprintf(stderr, "on_error err=%d msg=%s\n", errnum,
                (msg != nullptr) ? msg : "<null>");
}

TEST(BacktraceRustFfiSmoke, DumpFrames) {  // NOLINT
  g_frame_count = 0;
  int rc = ten_rust_backtrace_dump(nullptr, on_dump, on_error, 0);
  // rc 0 means not interrupted
  EXPECT_EQ(rc, 0);
  // expect at least some frames (different platforms have different numbers,
  // only verify >0)
  EXPECT_GT(g_frame_count, 0);
}

}  // namespace
