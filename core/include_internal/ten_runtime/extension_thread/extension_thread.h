//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_EXTENSION_THREAD_SIGNATURE 0xA1C756A818B47E1FU

#define EXTENSION_THREAD_QUEUE_SIZE 12800

typedef struct ten_extension_group_t ten_extension_group_t;
typedef struct ten_extension_store_t ten_extension_store_t;
typedef struct ten_extension_context_t ten_extension_context_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct MetricHandle MetricHandle;

typedef enum TEN_EXTENSION_THREAD_STATE {
  TEN_EXTENSION_THREAD_STATE_INIT,
  TEN_EXTENSION_THREAD_STATE_CREATING_EXTENSIONS,
  TEN_EXTENSION_THREAD_STATE_NORMAL,
  TEN_EXTENSION_THREAD_STATE_PREPARE_TO_CLOSE,

  // All extensions of this extension thread are closed, and removed from this
  // extension thread.
  TEN_EXTENSION_THREAD_STATE_CLOSED,
} TEN_EXTENSION_THREAD_STATE;

typedef struct ten_acquire_lock_mode_result_t {
  ten_event_t *completed;
  ten_error_t err;
} ten_acquire_lock_mode_result_t;

typedef struct ten_extension_thread_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_EXTENSION_THREAD_STATE state;
  bool is_close_triggered;

  ten_mutex_t *lock_mode_lock;
  bool in_lock_mode;

  ten_list_t pending_msgs_received_in_init_stage;

  ten_list_t extensions;  // ten_extension_t*
  size_t extensions_cnt_of_deleted;

  // Store all extensions (ten_extension_t*) belong to this extension thread.
  ten_extension_store_t *extension_store;

  // One extension thread corresponds to one extension group.
  ten_extension_group_t *extension_group;
  ten_extension_context_t *extension_context;

  ten_runloop_t *runloop;
  ten_event_t *runloop_is_ready_to_use;
} ten_extension_thread_t;

TEN_RUNTIME_API bool ten_extension_thread_not_call_by_me(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_extension_thread_call_by_me(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API bool
ten_extension_thread_check_integrity_if_in_lock_mode(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_extension_thread_check_integrity(
    ten_extension_thread_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_extension_thread_t *ten_extension_thread_create(
    void);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_attach_to_context_and_group(
    ten_extension_thread_t *self, ten_extension_context_t *extension_context,
    ten_extension_group_t *extension_group);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_destroy(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_start(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_close(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API TEN_EXTENSION_THREAD_STATE
ten_extension_thread_get_state(ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_set_state(
    ten_extension_thread_t *self, TEN_EXTENSION_THREAD_STATE state);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_call_all_extension_on_start(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_thread_add_all_created_extensions(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API ten_runloop_t *
ten_extension_thread_get_attached_runloop(ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_process_acquire_lock_mode_task(void *self_, void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_stop_life_cycle_of_all_extensions(
    ten_extension_thread_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_stop_life_cycle_of_all_extensions_task(void *self,
                                                            void *arg);

TEN_RUNTIME_PRIVATE_API void
ten_extension_thread_start_life_cycle_of_all_extensions_task(void *self_,
                                                             void *arg);
