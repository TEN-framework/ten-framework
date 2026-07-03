//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>
#include <stdint.h>

#include "include_internal/ten_runtime/common/loc.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"

#define TEN_TIMER_SIGNATURE 0x94DA8B352E91919DU

typedef struct ten_timer_t ten_timer_t;

typedef void (*ten_timer_on_trigger_func_t)(ten_timer_t *self,
                                            void *on_trigger_data);

typedef void (*ten_timer_on_closed_func_t)(ten_timer_t *self,
                                           void *on_closed_data);

struct ten_timer_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_atomic_t is_closing;

  ten_timer_on_closed_func_t on_closed;
  void *on_closed_data;

  ten_timer_on_trigger_func_t on_trigger;
  void *on_trigger_data;

  uint32_t id;
  uint64_t timeout_us;
  int32_t requested_times;  // TEN_TIMER_INFINITE means "forever"
  int32_t times;

  // If the auto_restart flag is set to be 'false', it will __not__
  // automatically restart timing after each timeout. Instead, the user needs to
  // manually restart the timer (ten_timer_enable). When the number of timeouts
  // exceeds the specified times, the timer will automatically close.
  //
  // Conversely, if auto_restart is set to be 'true' (by default), the timer
  // will automatically decide whether to restart timing or close the timer
  // based on its policy after each timeout.
  bool auto_restart;

  ten_loc_t src_loc;

  ten_runloop_timer_t *backend;
  ten_runloop_t *runloop;
};

TEN_RUNTIME_PRIVATE_API bool ten_timer_check_integrity(ten_timer_t *self,
                                                       bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_timer_t *ten_timer_create_with_cmd(
    ten_shared_ptr_t *cmd, ten_runloop_t *runloop);

TEN_RUNTIME_PRIVATE_API ten_timer_t *ten_timer_create(ten_runloop_t *runloop,
                                                      uint64_t timeout_us,
                                                      int32_t requested_times,
                                                      bool auto_restart);

TEN_RUNTIME_PRIVATE_API void ten_timer_destroy(ten_timer_t *self);

TEN_RUNTIME_PRIVATE_API void ten_timer_enable(ten_timer_t *self);

TEN_RUNTIME_PRIVATE_API void ten_timer_stop_async(ten_timer_t *self);

TEN_RUNTIME_PRIVATE_API void ten_timer_set_on_triggered(
    ten_timer_t *self, ten_timer_on_trigger_func_t on_trigger,
    void *on_trigger_data);

TEN_RUNTIME_PRIVATE_API void ten_timer_set_on_closed(
    ten_timer_t *self, ten_timer_on_closed_func_t on_closed,
    void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_timer_close_async(ten_timer_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_timer_is_id_equal_to(ten_timer_t *self,
                                                      uint32_t id);
