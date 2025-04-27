//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <uv.h>

#include "ten_utils/io/general/transport/backend/base.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"

#define TEN_STREAMBACKEND_TCP_SIGNATURE 0x861D0758EA843914U

typedef struct ten_transport_t ten_transport_t;

typedef struct ten_streambackend_tcp_t {
  ten_streambackend_t base;

  ten_atomic_t signature;

  uv_stream_t *uv_stream;
} ten_streambackend_tcp_t;

TEN_UTILS_PRIVATE_API ten_stream_t *ten_stream_tcp_create_uv(uv_loop_t *loop);

TEN_UTILS_PRIVATE_API void ten_streambackend_tcp_dump_info(
    ten_streambackend_tcp_t *tcp_stream, const char *fmt, ...);
