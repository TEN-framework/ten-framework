//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/binding/common.h"
#include "include_internal/ten_runtime/extension/msg_dest_info/all_msg_type_dest_info.h"
#include "include_internal/ten_runtime/extension/path_timer.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/path/result_return_policy.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/common/status_code.h"
#include "ten_runtime/extension/extension.h"
#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"

#define TEN_EXTENSION_SIGNATURE 0xE1627776E09A723CU

// In most modern operating systems, `-1` is not a valid user-space address.
// Therefore, we use this tricky approach to represent the value of a pointer to
// an extension that was not successfully created.
#define TEN_EXTENSION_UNSUCCESSFULLY_CREATED ((ten_extension_t *)-1)

#define TEN_EXTENSION_ON_XXX_WARNING_THRESHOLD_MS 100  // milli-seconds

typedef struct ten_env_t ten_env_t;
typedef struct ten_extension_t ten_extension_t;
typedef struct ten_audio_frame_t ten_audio_frame_t;
typedef struct ten_video_frame_t ten_video_frame_t;
typedef struct ten_extension_info_t ten_extension_info_t;
typedef struct ten_extension_context_t ten_extension_context_t;
typedef struct ten_extension_group_t ten_extension_group_t;
typedef struct ten_extension_thread_t ten_extension_thread_t;
typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_path_table_t ten_path_table_t;
typedef struct ten_path_in_t ten_path_in_t;
typedef struct ten_timer_t ten_timer_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_app_t ten_app_t;

// The relationship between several lifecycle stages and their connection to
// sending messages:
//
// - on_configure ~ on_configure_done + on_init ~ on_init_done: Handles its own
//   initialization; cannot send or receive messages. The reason for this is
//   that, before `on_init_done`, the extension may not be ready to handle
//   external requests, so the received messages need to be temporarily stored.
//
// - ~ on_start: The messages received before on_start() will be temporarily
//   stored, and only after on_start() is called will they be sent to the
//   extension. The reason for this is developers generally expect `on_start` to
//   occur before any `on_cmd` events.
//
// - on_start ~ on_stop_done: Normal sending and receiving of all
//   messages and results.
//
// - on_deinit ~ on_deinit_done: Handles its own de-initialization; cannot
//   receive messages.
typedef enum TEN_EXTENSION_STATE {
  TEN_EXTENSION_STATE_INIT,

  // on_configure() is called.
  TEN_EXTENSION_STATE_ON_CONFIGURE,

  // on_configure_done() is completed.
  TEN_EXTENSION_STATE_ON_CONFIGURE_DONE,

  // on_init() is called.
  TEN_EXTENSION_STATE_ON_INIT,

  // on_init_done() is completed.
  TEN_EXTENSION_STATE_ON_INIT_DONE,

  // on_start() is called.
  TEN_EXTENSION_STATE_ON_START,

  // on_start_done() is completed.
  TEN_EXTENSION_STATE_ON_START_DONE,

  // The extension is preparing to stop.
  TEN_EXTENSION_STATE_PREPARE_TO_STOP,

  // on_stop() is called.
  TEN_EXTENSION_STATE_ON_STOP,

  // on_stop_done() is completed.
  TEN_EXTENSION_STATE_ON_STOP_DONE,

  // on_deinit() is called.
  TEN_EXTENSION_STATE_ON_DEINIT,

  // on_deinit_done() is called.
  TEN_EXTENSION_STATE_ON_DEINIT_DONE,
} TEN_EXTENSION_STATE;

// Manual trigger life cycle stage enumeration
typedef enum TEN_MANUAL_TRIGGER_STAGE {
  TEN_MANUAL_TRIGGER_STAGE_START,
  TEN_MANUAL_TRIGGER_STAGE_STOP,

  TEN_MANUAL_TRIGGER_STAGE_MAX
} TEN_MANUAL_TRIGGER_STAGE;

// Manual trigger life cycle configuration
typedef struct ten_manual_trigger_life_cycle_t {
  // true if stage should be manually triggered
  bool stages[TEN_MANUAL_TRIGGER_STAGE_MAX];
} ten_manual_trigger_life_cycle_t;

struct ten_extension_t {
  ten_binding_handle_t binding_handle;

  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  TEN_EXTENSION_STATE state;

  // @{
  // Public interface.
  //
  // These public APIs are all async behaviors, that is to say, addon needs to
  // actively call on_xxx_done function to notify the TEN runtime that it is
  // done. But in some language bindings (such as JavaScript), because of
  // language-level asynchronous support (such as async/await), TEN runtime can
  // only provide async syntax API to addons (such as `async function onCmd`).
  // Addons can write async codes or sync codes in such async syntax API; but in
  // other language bindings, if TEN runtime wants to help addons do
  // on_xxx_done action at the end of onXxx, TEN runtime needs to provide sync
  // API to addons, such that onXxxSync().

  /**
   * @brief on_configure () must be the first public interface function of an
   * extension to be called.
   *
   * @note Extension can _not_ interact with other extensions (ex: send_cmd)
   * in its on_configure() stage.
   */
  ten_extension_on_configure_func_t on_configure;

  /**
   * @brief Extension can initialize itself in its on_init(). After on_init() is
   * completed, the TEN runtime will think that the extension can start to
   * respond to commands/data/audio-frames/video-frames.
   *
   * @note Extension can _not_ interact with other extensions (ex: send_cmd)
   * in its on_init() stage.
   */
  ten_extension_on_init_func_t on_init;

  /**
   * @brief on_start() is _not_ always be called before on_cmd(). on_start() can
   * be seen as when a graph is started, it will trigger some operations of the
   * extension. And at the same time, the on_start() of other extensions will
   * also cause the execution of the on_cmd() of the current extension.
   *
   * @note Extension can start to interact with other extensions (ex: send_cmd)
   * in its on_start() stage.
   */
  ten_extension_on_start_func_t on_start;

  /**
   * @note Extension can _still_ interact with other extensions (ex: send_cmd)
   * in its on_stop() stage.
   */
  ten_extension_on_stop_func_t on_stop;

  /**
   * @note Extension can _not_ interact with other extensions (ex: send_cmd) in
   * its on_deinit() stage.
   */
  ten_extension_on_deinit_func_t on_deinit;

  ten_extension_on_cmd_func_t on_cmd;
  ten_extension_on_data_func_t on_data;
  ten_extension_on_audio_frame_func_t on_audio_frame;
  ten_extension_on_video_frame_func_t on_video_frame;
  // @}

  ten_addon_host_t *addon_host;
  ten_string_t name;

  ten_env_t *ten_env;

  ten_extension_thread_t *extension_thread;
  ten_hashhandle_t hh_in_extension_store;

  ten_extension_context_t *extension_context;
  ten_extension_info_t *extension_info;

  // This is solely for caching the engine pointer, making it convenient to
  // access the engine pointer within the extension.
  ten_engine_t *engine;
  // This is solely for caching the app pointer, making it convenient to access
  // the app pointer within the extension.
  ten_app_t *app;

  ten_value_t manifest;
  ten_value_t property;

  ten_schema_store_t schema_store;

  ten_metadata_info_t *manifest_info;
  ten_metadata_info_t *property_info;

  // @{
  // The following 'pending_msgs_received_before_on_init_done' is used to keep
  // the received messages before the extension is inited completely.
  //
  // If the state of the extension is earlier than TEN_EXTENSION_STATE_ON_INIT,
  // the received messages will be kept in the
  // 'pending_msgs_received_before_on_init_done'. Once the `on_init_done()` is
  // called, the messages in the 'pending_msgs_received_before_on_init_done'
  // will be handled.
  //
  // As an exception, the 'cmd result' will be handled normally even if the
  // extension is not inited.
  ten_list_t pending_msgs_received_before_on_init_done;
  // @}

  ten_path_table_t *path_table;

  // @{
  // The following 'path_timers' is a list of ten_timer_t, each of which is used
  // to check whether paths in the path table are expired and remove them from
  // the path table.
  //
  // The size of the 'path_timers' could be the following:
  // - 0
  // - 1 (timer used to handle the in_path __or__ out_path)
  // - 2 (timers used to handle in_path __and__ out_path).
  ten_list_t path_timers;

  // This field is used to store the timeout duration of the in_path and
  // out_path.
  ten_path_timeout_info path_timeout_info;
  // @}

  // Records the number of occurrences of the error code
  // `TEN_ERROR_CODE_MSG_NOT_CONNECTED` for each message name when sending
  // output messages.
  ten_hashtable_t msg_not_connected_count_map;

  bool is_standalone_test_extension;

  // Manual trigger life cycle configuration
  ten_manual_trigger_life_cycle_t manual_trigger_life_cycle;

  // List of pending trigger_life_cycle commands waiting for response
  // Contains ten_shared_ptr_t* of trigger_life_cycle commands
  ten_list_t pending_trigger_life_cycle_cmds;

  void *user_data;
};

TEN_RUNTIME_PRIVATE_API void ten_extension_on_init(ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_start(ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_stop(ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_deinit(ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_cmd(ten_extension_t *self,
                                                  ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_data(ten_extension_t *self,
                                                   ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_video_frame(
    ten_extension_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_extension_on_audio_frame(
    ten_extension_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_extension_load_metadata(ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API void ten_extension_set_addon(
    ten_extension_t *self, ten_addon_host_t *addon_host);

TEN_RUNTIME_PRIVATE_API bool ten_extension_dispatch_msg(
    ten_extension_t *extension, ten_shared_ptr_t *msg,
    TEN_RESULT_RETURN_POLICY result_return_policy, ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_extension_get_attached_runloop(
    ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API const char *ten_extension_get_name(
    ten_extension_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_addon_host_t *ten_extension_get_addon(
    ten_extension_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_extension_validate_msg_schema(
    ten_extension_t *self, ten_shared_ptr_t *msg, bool is_msg_out,
    ten_error_t *err);

TEN_RUNTIME_PRIVATE_API ten_extension_t *ten_extension_from_smart_ptr(
    ten_smart_ptr_t *extension_smart_ptr);

TEN_RUNTIME_PRIVATE_API void ten_extension_handle_trigger_life_cycle_cmd(
    ten_extension_t *self, ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API bool ten_extension_has_pending_trigger_life_cycle_cmds(
    ten_extension_t *self, const char *stage);

TEN_RUNTIME_PRIVATE_API void
ten_extension_reply_pending_trigger_life_cycle_cmds_by_stage(
    ten_extension_t *self, const char *stage, TEN_STATUS_CODE status_code);

TEN_RUNTIME_PRIVATE_API void ten_extension_trigger_stop_if_needed(
    ten_extension_t *self);

TEN_RUNTIME_API void ten_extension_set_me_in_target_lang(
    ten_extension_t *self, void *me_in_target_lang);

static inline ten_engine_t *ten_extension_get_belonging_engine(
    ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  return self->engine;
}

static inline ten_app_t *ten_extension_get_belonging_app(
    ten_extension_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true),
             "Invalid use of extension %p.", self);

  return self->app;
}
