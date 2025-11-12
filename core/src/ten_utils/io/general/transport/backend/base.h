//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/string.h"

typedef struct ten_transport_t ten_transport_t;
typedef struct ten_transportbackend_t ten_transportbackend_t;

struct ten_transportbackend_t {
  ten_atomic_t is_close;
  const char *impl;
  ten_string_t *name;
  ten_transport_t *transport;

  int (*connect)(ten_transportbackend_t *backend, const ten_string_t *dest);
  int (*listen)(ten_transportbackend_t *backend, const ten_string_t *dest);
  void (*close)(ten_transportbackend_t *backend);
};

typedef struct ten_transportbackend_factory_t {
  ten_transportbackend_t *(*create)(ten_transport_t *transport,
                                    const ten_string_t *name);
} ten_transportbackend_factory_t;

TEN_UTILS_PRIVATE_API void ten_transportbackend_init(
    ten_transportbackend_t *self, ten_transport_t *transport,
    const ten_string_t *name);

TEN_UTILS_PRIVATE_API void ten_transportbackend_deinit(
    ten_transportbackend_t *self);

typedef struct ten_stream_t ten_stream_t;
typedef struct ten_streambackend_t ten_streambackend_t;

struct ten_streambackend_t {
  ten_atomic_t is_close;
  ten_stream_t *stream;
  const char *impl;

  int (*start_read)(ten_streambackend_t *self);
  int (*stop_read)(ten_streambackend_t *self);

  int (*write)(ten_streambackend_t *self, const void *buf, size_t size,
               void *user_data);

  int (*close)(ten_streambackend_t *self);
};

TEN_UTILS_PRIVATE_API void ten_streambackend_init(const char *impl,
                                                  ten_streambackend_t *backend,
                                                  ten_stream_t *stream);

TEN_UTILS_PRIVATE_API void ten_streambackend_deinit(
    ten_streambackend_t *backend);
