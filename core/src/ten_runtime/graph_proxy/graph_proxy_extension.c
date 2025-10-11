//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension_context/extension_context.h"
#include "include_internal/ten_runtime/extension_group/extension_group.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/ten.h"
#include "ten_runtime/ten_env/internal/log.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

typedef struct graph_proxy_context_t {
  ten_loc_t host_loc;
  ten_string_t current_graph_id;
} graph_proxy_context_t;

static graph_proxy_context_t *graph_proxy_context_create(void) {
  graph_proxy_context_t *context =
      (graph_proxy_context_t *)TEN_MALLOC(sizeof(graph_proxy_context_t));
  TEN_ASSERT(context, "Failed to allocate memory for graph proxy context.");

  ten_loc_init_empty(&context->host_loc);
  TEN_STRING_INIT(context->current_graph_id);

  return context;
}

static void graph_proxy_context_destroy(graph_proxy_context_t *context) {
  TEN_ASSERT(context, "Invalid argument.");

  ten_loc_deinit(&context->host_loc);
  ten_string_deinit(&context->current_graph_id);
  TEN_FREE(context);
}

// Helper function to determine if message should be forwarded to host_loc
static bool should_forward_to_host(graph_proxy_context_t *context,
                                   ten_shared_ptr_t *msg) {
  TEN_ASSERT(context, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");

  const char *src_graph_id = ten_msg_get_src_graph_id(msg);
  const char *current_graph_id =
      ten_string_get_raw_str(&context->current_graph_id);

  // If source graph id is the same as current graph id, forward to host_loc
  if (src_graph_id && current_graph_id &&
      strcmp(src_graph_id, current_graph_id) == 0) {
    return true;
  }

  return false;
}

// Helper function to set destination to host_loc
static bool set_dest_to_host_loc(graph_proxy_context_t *context,
                                 ten_shared_ptr_t *msg) {
  TEN_ASSERT(context, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Clear existing destinations and set destination to host_loc
  bool rc = ten_msg_clear_and_set_dest(
      msg, ten_string_get_raw_str(&context->host_loc.app_uri),
      ten_string_get_raw_str(&context->host_loc.graph_id),
      ten_string_get_raw_str(&context->host_loc.extension_name), &err);

  if (!rc) {
    TEN_LOGE("Failed to set destination to host_loc: %s",
             ten_error_message(&err));
  }

  ten_error_deinit(&err);
  return rc;
}

static void graph_proxy_extension_on_configure(ten_extension_t *self,
                                               ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");

  ten_env_on_configure_done(ten_env, NULL);
}

static void graph_proxy_extension_on_init(ten_extension_t *self,
                                          ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  graph_proxy_context_t *context = (graph_proxy_context_t *)self->user_data;
  TEN_ASSERT(context, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Get the host_loc property
  ten_value_t *host_loc_value =
      ten_env_peek_property(ten_env, TEN_STR_HOST_LOC, &err);
  if (!host_loc_value) {
    TEN_LOGI("Host_loc property not found in graph proxy extension: %s",
             ten_error_message(&err));
    ten_error_deinit(&err);
    ten_env_on_init_done(ten_env, NULL);
    return;
  }

  if (ten_value_get_type(host_loc_value) != TEN_TYPE_OBJECT) {
    TEN_LOGE("Host_loc property must be an object in graph proxy extension: %s",
             ten_error_message(&err));
    ten_error_deinit(&err);
    ten_env_on_init_done(ten_env, NULL);
    return;
  }

  // Extract app_uri
  ten_value_t *app_uri_value =
      ten_value_object_peek(host_loc_value, TEN_STR_APP);
  if (app_uri_value && ten_value_get_type(app_uri_value) == TEN_TYPE_STRING) {
    const char *app_uri_str = ten_value_peek_raw_str(app_uri_value, NULL);
    if (app_uri_str) {
      ten_loc_set_app_uri(&context->host_loc, app_uri_str);
    }
  } else {
    TEN_LOGE("Host_loc.app_uri must be a string in graph proxy extension: %s",
             ten_error_message(&err));
  }

  // Extract graph_id
  ten_value_t *graph_id_value =
      ten_value_object_peek(host_loc_value, TEN_STR_GRAPH);
  if (graph_id_value && ten_value_get_type(graph_id_value) == TEN_TYPE_STRING) {
    const char *graph_id_str = ten_value_peek_raw_str(graph_id_value, NULL);
    if (graph_id_str) {
      ten_loc_set_graph_id(&context->host_loc, graph_id_str);
    }
  }

  // Extract extension_name
  ten_value_t *extension_name_value =
      ten_value_object_peek(host_loc_value, TEN_STR_EXTENSION);
  if (extension_name_value &&
      ten_value_get_type(extension_name_value) == TEN_TYPE_STRING) {
    const char *extension_name_str =
        ten_value_peek_raw_str(extension_name_value, NULL);
    if (extension_name_str) {
      ten_loc_set_extension_name(&context->host_loc, extension_name_str);
    }
  }

  // Get current extension's graph id
  ten_extension_t *extension = ten_env_get_attached_extension(ten_env);
  TEN_ASSERT(extension, "Should not happen.");
  TEN_ASSERT(ten_extension_check_integrity(extension, true),
             "Should not happen.");

  ten_extension_thread_t *extension_thread = extension->extension_thread;
  TEN_ASSERT(extension_thread, "Should not happen.");
  TEN_ASSERT(ten_extension_thread_check_integrity(extension_thread, true),
             "Should not happen.");

  ten_extension_context_t *extension_context =
      extension_thread->extension_context;
  TEN_ASSERT(extension_context, "Should not happen.");
  TEN_ASSERT(ten_extension_context_check_integrity(extension_context, false),
             "Should not happen.");

  ten_engine_t *engine = extension_context->engine;
  TEN_ASSERT(engine, "Should not happen.");
  TEN_ASSERT(ten_engine_check_integrity(engine, false), "Should not happen.");

  ten_string_set_from_c_str(&context->current_graph_id,
                            ten_engine_get_id(engine, false));

  TEN_LOGI(
      "Graph proxy extension initialized with host_loc: app_uri=%s, "
      "graph_id=%s, extension_name=%s, current_graph_id=%s",
      ten_string_get_raw_str(&context->host_loc.app_uri),
      ten_string_get_raw_str(&context->host_loc.graph_id),
      ten_string_get_raw_str(&context->host_loc.extension_name),
      ten_string_get_raw_str(&context->current_graph_id));

  ten_env_on_init_done(ten_env, NULL);
}

static void graph_proxy_extension_on_start(ten_extension_t *self,
                                           ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_env_on_start_done(ten_env, NULL);
}

static void graph_proxy_extension_on_stop(ten_extension_t *self,
                                          ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_env_on_stop_done(ten_env, NULL);
}

static void graph_proxy_extension_on_deinit(ten_extension_t *self,
                                            ten_env_t *ten_env) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_env_on_deinit_done(ten_env, NULL);
}

static void graph_proxy_extension_on_cmd(ten_extension_t *self,
                                         ten_env_t *ten_env,
                                         ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(cmd, "Invalid argument.");

  graph_proxy_context_t *graph_proxy_context =
      (graph_proxy_context_t *)self->user_data;
  TEN_ASSERT(graph_proxy_context, "Should not happen.");

  if (should_forward_to_host(graph_proxy_context, cmd)) {
    // Forward to host_loc
    if (set_dest_to_host_loc(graph_proxy_context, cmd)) {
      ten_error_t err;
      TEN_ERROR_INIT(err);

      bool rc = ten_env_send_cmd(ten_env, cmd, NULL, NULL, NULL, &err);
      if (!rc) {
        TEN_LOGE("Failed to send cmd to host_loc: %s", ten_error_message(&err));
      }

      ten_error_deinit(&err);
    }
  } else {
    // Bypass - send directly
    ten_error_t err;
    TEN_ERROR_INIT(err);

    bool rc = ten_env_send_cmd(ten_env, cmd, NULL, NULL, NULL, &err);
    if (!rc) {
      TEN_LOGE("Failed to bypass cmd: %s", ten_error_message(&err));
    }

    ten_error_deinit(&err);
  }
}

static void graph_proxy_extension_on_data(ten_extension_t *self,
                                          ten_env_t *ten_env,
                                          ten_shared_ptr_t *data) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(data, "Invalid argument.");

  graph_proxy_context_t *graph_proxy_context =
      (graph_proxy_context_t *)self->user_data;
  TEN_ASSERT(graph_proxy_context, "Should not happen.");

  if (should_forward_to_host(graph_proxy_context, data)) {
    // Forward to host_loc
    if (set_dest_to_host_loc(graph_proxy_context, data)) {
      ten_error_t err;
      TEN_ERROR_INIT(err);

      bool rc = ten_env_send_data(ten_env, data, NULL, NULL, &err);
      if (!rc) {
        TEN_LOGE("Failed to send data to host_loc: %s",
                 ten_error_message(&err));
      }

      ten_error_deinit(&err);
    }
  } else {
    // Bypass - send directly
    ten_error_t err;
    TEN_ERROR_INIT(err);

    bool rc = ten_env_send_data(ten_env, data, NULL, NULL, &err);
    if (!rc) {
      TEN_LOGE("Failed to bypass data: %s", ten_error_message(&err));
    }

    ten_error_deinit(&err);
  }
}

static void graph_proxy_extension_on_audio_frame(ten_extension_t *self,
                                                 ten_env_t *ten_env,
                                                 ten_shared_ptr_t *frame) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(frame, "Invalid argument.");

  graph_proxy_context_t *graph_proxy_context =
      (graph_proxy_context_t *)self->user_data;
  TEN_ASSERT(graph_proxy_context, "Should not happen.");

  if (should_forward_to_host(graph_proxy_context, frame)) {
    // Forward to host_loc
    if (set_dest_to_host_loc(graph_proxy_context, frame)) {
      ten_error_t err;
      TEN_ERROR_INIT(err);

      bool rc = ten_env_send_audio_frame(ten_env, frame, NULL, NULL, &err);
      if (!rc) {
        TEN_LOGE("Failed to send audio_frame to host_loc: %s",
                 ten_error_message(&err));
      }

      ten_error_deinit(&err);
    }
  } else {
    // Bypass - send directly
    ten_error_t err;
    TEN_ERROR_INIT(err);

    bool rc = ten_env_send_audio_frame(ten_env, frame, NULL, NULL, &err);
    if (!rc) {
      TEN_LOGE("Failed to bypass audio_frame: %s", ten_error_message(&err));
    }

    ten_error_deinit(&err);
  }
}

static void graph_proxy_extension_on_video_frame(ten_extension_t *self,
                                                 ten_env_t *ten_env,
                                                 ten_shared_ptr_t *frame) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_extension_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(frame, "Invalid argument.");

  graph_proxy_context_t *graph_proxy_context =
      (graph_proxy_context_t *)self->user_data;
  TEN_ASSERT(graph_proxy_context, "Should not happen.");

  if (should_forward_to_host(graph_proxy_context, frame)) {
    // Forward to host_loc
    if (set_dest_to_host_loc(graph_proxy_context, frame)) {
      ten_error_t err;
      TEN_ERROR_INIT(err);

      bool rc = ten_env_send_video_frame(ten_env, frame, NULL, NULL, &err);
      if (!rc) {
        TEN_LOGE("Failed to send video_frame to host_loc: %s",
                 ten_error_message(&err));
      }

      ten_error_deinit(&err);
    }
  } else {
    // Bypass - send directly
    ten_error_t err;
    TEN_ERROR_INIT(err);

    bool rc = ten_env_send_video_frame(ten_env, frame, NULL, NULL, &err);
    if (!rc) {
      TEN_LOGE("Failed to bypass video_frame: %s", ten_error_message(&err));
    }

    ten_error_deinit(&err);
  }
}

static void graph_proxy_extension_addon_create_instance(ten_addon_t *addon,
                                                        ten_env_t *ten_env,
                                                        const char *name,
                                                        void *context) {
  TEN_ASSERT(addon, "Invalid argument.");
  TEN_ASSERT(name, "Invalid argument.");

  ten_extension_t *extension = ten_extension_create(
      name, graph_proxy_extension_on_configure, graph_proxy_extension_on_init,
      graph_proxy_extension_on_start, graph_proxy_extension_on_stop,
      graph_proxy_extension_on_deinit, graph_proxy_extension_on_cmd,
      graph_proxy_extension_on_data, graph_proxy_extension_on_audio_frame,
      graph_proxy_extension_on_video_frame, NULL);

  // Create the context
  graph_proxy_context_t *graph_proxy_context = graph_proxy_context_create();
  TEN_ASSERT(graph_proxy_context, "Failed to allocate memory.");
  extension->user_data = graph_proxy_context;

  ten_env_on_create_instance_done(ten_env, extension, context, NULL);
}

static void graph_proxy_extension_addon_destroy_instance(
    TEN_UNUSED ten_addon_t *addon, ten_env_t *ten_env, void *_extension,
    void *context) {
  ten_extension_t *extension = (ten_extension_t *)_extension;
  TEN_ASSERT(extension, "Invalid argument.");

  graph_proxy_context_t *graph_proxy_context =
      (graph_proxy_context_t *)extension->user_data;
  TEN_ASSERT(graph_proxy_context, "Should not happen.");
  graph_proxy_context_destroy(graph_proxy_context);

  ten_extension_destroy(extension);

  ten_env_on_destroy_instance_done(ten_env, context, NULL);
}

static ten_addon_t ten_builtin_graph_proxy_extension_addon = {
    NULL,
    TEN_ADDON_SIGNATURE,
    NULL,
    graph_proxy_extension_addon_create_instance,
    graph_proxy_extension_addon_destroy_instance,
    NULL,
    NULL,
};

// Addon registration phase 2: actually registering the addon into the addon
// store.
static void ten_builtin_graph_proxy_extension_addon_register_handler(
    ten_addon_registration_t *registration,
    ten_addon_registration_done_func_t done_callback,
    ten_addon_register_ctx_t *register_ctx, void *user_data) {
  TEN_ASSERT(registration, "Invalid argument.");
  TEN_ASSERT(register_ctx, "Invalid argument.");
  TEN_ASSERT(registration->func, "Invalid argument.");

  ten_addon_register_extension(TEN_STR_TEN_GRAPH_PROXY_EXTENSION, NULL,
                               &ten_builtin_graph_proxy_extension_addon,
                               register_ctx);

  done_callback(register_ctx, user_data);
}

// This is the phase 1 of the addon registration process: adding a function,
// which will perform the actual registration in the phase 2, into the
// `addon_manager`.
void ten_addon_manager_add_builtin_graph_proxy_extension(
    ten_addon_manager_t *manager) {
  TEN_ASSERT(manager, "Invalid argument.");

  ten_addon_manager_add_addon(
      manager, "extension", TEN_STR_TEN_GRAPH_PROXY_EXTENSION,
      ten_builtin_graph_proxy_extension_addon_register_handler, NULL, NULL);
}
