//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/video_frame.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_video_frame_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_video_frame_register_class(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // video_frame constructor
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  napi_status status =
      napi_create_reference(env, args[0], 1, &js_video_frame_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(
        NULL, NAPI_AUTO_LENGTH,
        "Failed to create JS reference to JS video_frame constructor.",
        NAPI_AUTO_LENGTH);
    TEN_ASSERT(
        0, "Failed to create JS reference to JS video_frame constructor: %d",
        status);
  }

  return js_undefined(env);
}

static void ten_nodejs_video_frame_destroy(ten_nodejs_video_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_nodejs_msg_deinit(&self->msg);

  TEN_FREE(self);
}

static void ten_nodejs_video_frame_finalize(napi_env env, void *video_frame,
                                            void *hint) {
  ten_nodejs_video_frame_t *video_frame_bridge = video_frame;
  TEN_ASSERT(video_frame_bridge, "Should not happen.");

  napi_delete_reference(env, video_frame_bridge->msg.bridge.js_instance_ref);

  ten_nodejs_video_frame_destroy(video_frame_bridge);
}

static napi_value ten_nodejs_video_frame_create(napi_env env,
                                                napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, video_frame_name
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_string_t video_frame_name;
  TEN_STRING_INIT(video_frame_name);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &video_frame_name);
  if (!rc) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get video_frame_name.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_t error;
  ten_error_init(&error);

  ten_shared_ptr_t *c_video_frame =
      ten_video_frame_create(ten_string_get_raw_str(&video_frame_name), &error);
  TEN_ASSERT(c_video_frame, "Failed to create video_frame.");

  ten_string_deinit(&video_frame_name);

  ten_nodejs_video_frame_t *video_frame_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_video_frame_t));
  TEN_ASSERT(video_frame_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&video_frame_bridge->msg, c_video_frame);
  // Decrement the reference count of c_video_frame to indicate that the JS
  // video_frame takes the full ownership of this c_video_frame, in other words,
  // when the JS video_frame is finalized, its C video_frame would be destroyed,
  // too.
  ten_shared_ptr_destroy(c_video_frame);

  napi_status status = napi_wrap(
      env, args[0], video_frame_bridge, ten_nodejs_video_frame_finalize, NULL,
      &video_frame_bridge->msg.bridge.js_instance_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to wrap JS video_frame object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to wrap JS video_frame object: %d", status);
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_alloc_buf(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, size

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  uint32_t size = 0;
  status = napi_get_value_uint32(env, args[1], &size);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get size.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get size: %d", status);
  }

  ten_video_frame_alloc_data(video_frame_bridge->msg.msg, size);

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_lock_buf(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_msg_add_locked_res_buf(
          video_frame_bridge->msg.msg,
          ten_video_frame_peek_buf(video_frame_bridge->msg.msg)->data, &err)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to lock buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to lock buffer.");
  }

  ten_buf_t *buf = ten_video_frame_peek_buf(video_frame_bridge->msg.msg);

  napi_value js_buf = NULL;
  status = napi_create_external_arraybuffer(
      env, ten_buf_get_data(buf), ten_buf_get_size(buf), NULL, NULL, &js_buf);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create buffer: %d", status);
  }

  return js_buf;
}

static napi_value ten_nodejs_video_frame_unlock_buf(napi_env env,
                                                    napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, buffer

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  void *data = NULL;
  status = napi_get_arraybuffer_info(env, args[1], &data, NULL);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get buffer: %d", status);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_msg_remove_locked_res_buf(video_frame_bridge->msg.msg, data, &err)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to unlock buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unlock buffer.");
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_get_buf(napi_env env,
                                                 napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  ten_buf_t *buf = ten_video_frame_peek_buf(video_frame_bridge->msg.msg);
  if (!buf) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get buffer.");
  }

  napi_value js_buf = NULL;
  status = napi_create_buffer_copy(env, ten_buf_get_size(buf),
                                   ten_buf_get_data(buf), NULL, &js_buf);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create buffer: %d", status);
  }

  return js_buf;
}

static napi_value ten_nodejs_video_frame_get_width(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int32_t width = ten_video_frame_get_width(video_frame_bridge->msg.msg);

  napi_value js_width = NULL;
  status = napi_create_int32(env, width, &js_width);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create width.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create width: %d", status);
  }

  return js_width;
}

static napi_value ten_nodejs_video_frame_set_width(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, width

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int32_t width = 0;
  status = napi_get_value_int32(env, args[1], &width);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get width.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get width: %d", status);
  }

  ten_video_frame_set_width(video_frame_bridge->msg.msg, width);

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_get_height(napi_env env,
                                                    napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int32_t height = ten_video_frame_get_height(video_frame_bridge->msg.msg);

  napi_value js_height = NULL;
  status = napi_create_int32(env, height, &js_height);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create height.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create height: %d", status);
  }

  return js_height;
}

static napi_value ten_nodejs_video_frame_set_height(napi_env env,
                                                    napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, height

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int32_t height = 0;
  status = napi_get_value_int32(env, args[1], &height);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get height.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get height: %d", status);
  }

  ten_video_frame_set_height(video_frame_bridge->msg.msg, height);

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_get_timestamp(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int64_t timestamp =
      ten_video_frame_get_timestamp(video_frame_bridge->msg.msg);

  napi_value js_timestamp = NULL;
  status = napi_create_int64(env, timestamp, &js_timestamp);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create timestamp.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create timestamp: %d", status);
  }

  return js_timestamp;
}

static napi_value ten_nodejs_video_frame_set_timestamp(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, timestamp

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  int64_t timestamp = 0;
  status = napi_get_value_int64(env, args[1], &timestamp);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get timestamp.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get timestamp: %d", status);
  }

  ten_video_frame_set_timestamp(video_frame_bridge->msg.msg, timestamp);

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_get_pixel_fmt(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  TEN_PIXEL_FMT pixel_fmt =
      ten_video_frame_get_pixel_fmt(video_frame_bridge->msg.msg);

  napi_value js_pixel_fmt = NULL;
  status = napi_create_int32(env, pixel_fmt, &js_pixel_fmt);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create pixel_fmt.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create pixel_fmt: %d", status);
  }

  return js_pixel_fmt;
}

static napi_value ten_nodejs_video_frame_set_pixel_fmt(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, pixel_fmt

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  TEN_PIXEL_FMT pixel_fmt = 0;
  status = napi_get_value_int32(env, args[1], (int32_t *)&pixel_fmt);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get pixel_fmt.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get pixel_fmt: %d", status);
  }

  ten_video_frame_set_pixel_fmt(video_frame_bridge->msg.msg, pixel_fmt);

  return js_undefined(env);
}

static napi_value ten_nodejs_video_frame_is_eof(napi_env env,
                                                napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  bool is_eof = ten_video_frame_is_eof(video_frame_bridge->msg.msg);

  napi_value js_is_eof = NULL;
  status = napi_get_boolean(env, is_eof, &js_is_eof);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create is_eof.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create is_eof: %d", status);
  }

  return js_is_eof;
}

static napi_value ten_nodejs_video_frame_set_eof(napi_env env,
                                                 napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, is_eof

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_video_frame_t *video_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&video_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS video_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS video_frame object: %d", status);
  }

  bool is_eof = false;
  status = napi_get_value_bool(env, args[1], &is_eof);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get is_eof.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get is_eof: %d", status);
  }

  ten_video_frame_set_eof(video_frame_bridge->msg.msg, is_eof);

  return js_undefined(env);
}

napi_value ten_nodejs_video_frame_wrap(napi_env env,
                                       ten_shared_ptr_t *video_frame) {
  TEN_ASSERT(video_frame && ten_msg_check_integrity(video_frame),
             "Should not happen.");

  ten_nodejs_video_frame_t *video_frame_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_video_frame_t));
  TEN_ASSERT(video_frame_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&video_frame_bridge->msg, video_frame);

  napi_value js_msg_name = NULL;
  napi_value js_create_shell_only_flag = NULL;

  const char *msg_name = ten_msg_get_name(video_frame);
  TEN_ASSERT(msg_name, "Should not happen.");

  napi_status status =
      napi_create_string_utf8(env, msg_name, NAPI_AUTO_LENGTH, &js_msg_name);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_msg_name != NULL,
                      "Failed to create JS string: %d", status);

  status = napi_get_boolean(env, true, &js_create_shell_only_flag);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_create_shell_only_flag != NULL,
                      "Failed to create JS boolean: %d", status);

  napi_value argv[] = {js_msg_name, js_create_shell_only_flag};

  napi_value js_video_frame = ten_nodejs_create_new_js_object_and_wrap(
      env, js_video_frame_constructor_ref, video_frame_bridge,
      ten_nodejs_video_frame_finalize,
      &video_frame_bridge->msg.bridge.js_instance_ref, 2, argv);
  ASSERT_IF_NAPI_FAIL(js_video_frame != NULL,
                      "Failed to create JS video_frame object.");

  return js_video_frame;
}

napi_value ten_nodejs_video_frame_module_init(napi_env env,
                                              napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_create);

  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_alloc_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_lock_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_unlock_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_get_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_get_width);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_set_width);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_get_height);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_set_height);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_get_timestamp);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_set_timestamp);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_get_pixel_fmt);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_set_pixel_fmt);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_is_eof);
  EXPORT_FUNC(env, exports, ten_nodejs_video_frame_set_eof);

  return exports;
}
