//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_ASYNC_SIGNATURE 0xD4CD6DEDB7906C26U

typedef struct ten_async_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_string_t name;
  ten_atomic_t close;

  ten_runloop_t *loop;
  ten_runloop_async_t *async;
  ten_runloop_async_t *async_for_close;

  void (*on_trigger)(struct ten_async_t *, void *);
  void *on_trigger_data;

  void (*on_closed)(struct ten_async_t *, void *);
  void *on_closed_data;
} ten_async_t;

TEN_UTILS_API ten_async_t *ten_async_create(const char *name,
                                            ten_runloop_t *loop,
                                            void *on_trigger,
                                            void *on_trigger_data);

TEN_UTILS_API void ten_async_set_on_closed(ten_async_t *self, void *on_closed,
                                           void *on_closed_data);

TEN_UTILS_API void ten_async_trigger(ten_async_t *self);

TEN_UTILS_API void ten_async_close(ten_async_t *self);
