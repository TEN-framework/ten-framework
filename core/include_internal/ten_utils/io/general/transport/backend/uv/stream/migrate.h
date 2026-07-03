//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <uv.h>

#include "ten_utils/io/stream.h"

typedef struct ten_migrate_t {
  ten_stream_t *stream;

  ten_runloop_t *from;
  ten_runloop_t *to;

#if !defined(_WIN32)
  uv_os_sock_t fds[2];
#else
  uv_file fds[2];
#endif
  uv_pipe_t *pipe[2];

  int migrate_processed;

  ten_atomic_t expect_finalize_count;
  ten_atomic_t finalized_count;

  // @{
  // The following 2 'async' belong to the 'from' thread/runloop.
  uv_async_t src_prepare;
  uv_async_t src_migration;
  // @}

  // @{
  // The following 2 'async' belong to the 'to' thread/runloop.
  uv_async_t dst_prepare;
  uv_async_t dst_migration;
  // @}

  void **user_data;
  void (*migrated)(ten_stream_t *new_stream, void **user_data);
} ten_migrate_t;

TEN_UTILS_PRIVATE_API void migration_dst_prepare(uv_async_t *async);

TEN_UTILS_PRIVATE_API void migration_dst_start(uv_async_t *async);

TEN_UTILS_PRIVATE_API int ten_stream_migrate_uv_stage2(ten_migrate_t *migrate);
