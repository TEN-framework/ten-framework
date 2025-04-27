//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/msg/audio_frame.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_utils/macro/memory.h"

static napi_ref js_audio_frame_constructor_ref = NULL;  // NOLINT

static napi_value ten_nodejs_audio_frame_register_class(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // audio_frame constructor
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
    return NULL;
  }

  napi_status status =
      napi_create_reference(env, args[0], 1, &js_audio_frame_constructor_ref);
  if (status != napi_ok) {
    napi_fatal_error(
        NULL, NAPI_AUTO_LENGTH,
        "Failed to create JS reference to JS audio_frame constructor.",
        NAPI_AUTO_LENGTH);
    TEN_ASSERT(
        0, "Failed to create JS reference to JS audio_frame constructor: %d",
        status);
  }

  return js_undefined(env);
}

static void ten_nodejs_audio_frame_destroy(ten_nodejs_audio_frame_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_nodejs_msg_deinit(&self->msg);

  TEN_FREE(self);
}

static void ten_nodejs_audio_frame_finalize(napi_env env, void *audio_frame,
                                            void *hint) {
  ten_nodejs_audio_frame_t *audio_frame_bridge = audio_frame;
  TEN_ASSERT(audio_frame_bridge, "Should not happen.");

  napi_delete_reference(env, audio_frame_bridge->msg.bridge.js_instance_ref);

  ten_nodejs_audio_frame_destroy(audio_frame_bridge);
}

static napi_value ten_nodejs_audio_frame_create(napi_env env,
                                                napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, audio_frame_name
  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_string_t audio_frame_name;
  TEN_STRING_INIT(audio_frame_name);

  bool rc = ten_nodejs_get_str_from_js(env, args[1], &audio_frame_name);
  if (!rc) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get audio_frame_name.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_error_t error;
  ten_error_init(&error);

  ten_shared_ptr_t *c_audio_frame =
      ten_audio_frame_create(ten_string_get_raw_str(&audio_frame_name), &error);
  TEN_ASSERT(c_audio_frame, "Failed to create audio_frame.");

  ten_string_deinit(&audio_frame_name);

  ten_nodejs_audio_frame_t *audio_frame_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_audio_frame_t));
  TEN_ASSERT(audio_frame_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&audio_frame_bridge->msg, c_audio_frame);
  // Decrement the reference count of c_audio_frame to indicate that the JS
  // audio_frame takes the full ownership of this c_audio_frame, in other words,
  // when the JS audio_frame is finalized, its C audio_frame would be destroyed,
  // too.
  ten_shared_ptr_destroy(c_audio_frame);

  napi_status status = napi_wrap(
      env, args[0], audio_frame_bridge, ten_nodejs_audio_frame_finalize, NULL,
      &audio_frame_bridge->msg.bridge.js_instance_ref);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to wrap JS audio_frame object.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to wrap JS audio_frame object: %d", status);
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_alloc_buf(napi_env env,
                                                   napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, size

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  uint32_t size = 0;
  status = napi_get_value_uint32(env, args[1], &size);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get size.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get size: %d", status);
  }

  ten_audio_frame_alloc_buf(audio_frame_bridge->msg.msg, size);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_lock_buf(napi_env env,
                                                  napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_msg_add_locked_res_buf(
          audio_frame_bridge->msg.msg,
          ten_audio_frame_peek_buf(audio_frame_bridge->msg.msg)->data, &err)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to lock buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to lock buffer.");
  }

  ten_buf_t *buf = ten_audio_frame_peek_buf(audio_frame_bridge->msg.msg);

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

static napi_value ten_nodejs_audio_frame_unlock_buf(napi_env env,
                                                    napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, buffer

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
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

  if (!ten_msg_remove_locked_res_buf(audio_frame_bridge->msg.msg, data, &err)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to unlock buffer.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unlock buffer.");
  }

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_buf(napi_env env,
                                                 napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  ten_buf_t *buf = ten_audio_frame_peek_buf(audio_frame_bridge->msg.msg);
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

static napi_value ten_nodejs_audio_frame_get_timestamp(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int64_t timestamp =
      ten_audio_frame_get_timestamp(audio_frame_bridge->msg.msg);

  napi_value js_timestamp = NULL;
  status = napi_create_int64(env, timestamp, &js_timestamp);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create timestamp.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create timestamp: %d", status);
  }

  return js_timestamp;
}

static napi_value ten_nodejs_audio_frame_set_timestamp(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, timestamp

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int64_t timestamp = 0;
  status = napi_get_value_int64(env, args[1], &timestamp);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get timestamp.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get timestamp: %d", status);
  }

  ten_audio_frame_set_timestamp(audio_frame_bridge->msg.msg, timestamp);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_sample_rate(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t sample_rate =
      ten_audio_frame_get_sample_rate(audio_frame_bridge->msg.msg);

  napi_value js_sample_rate = NULL;
  status = napi_create_int32(env, sample_rate, &js_sample_rate);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create sample_rate.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create sample_rate: %d", status);
  }

  return js_sample_rate;
}

static napi_value ten_nodejs_audio_frame_set_sample_rate(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, sample_rate

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t sample_rate = 0;
  status = napi_get_value_int32(env, args[1], &sample_rate);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get sample_rate.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get sample_rate: %d", status);
  }

  ten_audio_frame_set_sample_rate(audio_frame_bridge->msg.msg, sample_rate);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_samples_per_channel(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t samples_per_channel =
      ten_audio_frame_get_samples_per_channel(audio_frame_bridge->msg.msg);

  napi_value js_samples_per_channel = NULL;
  status = napi_create_int32(env, samples_per_channel, &js_samples_per_channel);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create samples_per_channel.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create samples_per_channel: %d", status);
  }

  return js_samples_per_channel;
}

static napi_value ten_nodejs_audio_frame_set_samples_per_channel(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, samples_per_channel

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t samples_per_channel = 0;
  status = napi_get_value_int32(env, args[1], &samples_per_channel);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to get samples_per_channel.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get samples_per_channel: %d", status);
  }

  ten_audio_frame_set_samples_per_channel(audio_frame_bridge->msg.msg,
                                          samples_per_channel);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_bytes_per_sample(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t bytes_per_sample =
      ten_audio_frame_get_bytes_per_sample(audio_frame_bridge->msg.msg);

  napi_value js_bytes_per_sample = NULL;
  status = napi_create_int32(env, bytes_per_sample, &js_bytes_per_sample);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create bytes_per_sample.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create bytes_per_sample: %d", status);
  }

  return js_bytes_per_sample;
}

static napi_value ten_nodejs_audio_frame_set_bytes_per_sample(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, bytes_per_sample

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t bytes_per_sample = 0;
  status = napi_get_value_int32(env, args[1], &bytes_per_sample);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get bytes_per_sample.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get bytes_per_sample: %d", status);
  }

  ten_audio_frame_set_bytes_per_sample(audio_frame_bridge->msg.msg,
                                       bytes_per_sample);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_number_of_channels(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t number_of_channels =
      ten_audio_frame_get_number_of_channel(audio_frame_bridge->msg.msg);

  napi_value js_number_of_channels = NULL;
  status = napi_create_int32(env, number_of_channels, &js_number_of_channels);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to create number_of_channels.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create number_of_channels: %d", status);
  }

  return js_number_of_channels;
}

static napi_value ten_nodejs_audio_frame_set_number_of_channels(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, number_of_channels

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t number_of_channels = 0;
  status = napi_get_value_int32(env, args[1], &number_of_channels);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to get number_of_channels.", NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get number_of_channels: %d", status);
  }

  ten_audio_frame_set_number_of_channel(audio_frame_bridge->msg.msg,
                                        number_of_channels);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_data_fmt(napi_env env,
                                                      napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  TEN_AUDIO_FRAME_DATA_FMT data_fmt =
      ten_audio_frame_get_data_fmt(audio_frame_bridge->msg.msg);

  napi_value js_data_fmt = NULL;
  status = napi_create_int32(env, data_fmt, &js_data_fmt);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create data_fmt.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create data_fmt: %d", status);
  }

  return js_data_fmt;
}

static napi_value ten_nodejs_audio_frame_set_data_fmt(napi_env env,
                                                      napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, data_fmt

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  TEN_AUDIO_FRAME_DATA_FMT data_fmt = TEN_AUDIO_FRAME_DATA_FMT_INVALID;
  status = napi_get_value_int32(env, args[1], (int32_t *)&data_fmt);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get data_fmt.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get data_fmt: %d", status);
  }

  ten_audio_frame_set_data_fmt(audio_frame_bridge->msg.msg, data_fmt);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_get_line_size(
    napi_env env, napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t line_size =
      ten_audio_frame_get_line_size(audio_frame_bridge->msg.msg);

  napi_value js_line_size = NULL;
  status = napi_create_int32(env, line_size, &js_line_size);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create line_size.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create line_size: %d", status);
  }

  return js_line_size;
}

static napi_value ten_nodejs_audio_frame_set_line_size(
    napi_env env, napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, line_size

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  int32_t line_size = 0;
  status = napi_get_value_int32(env, args[1], &line_size);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get line_size.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get line_size: %d", status);
  }

  ten_audio_frame_set_line_size(audio_frame_bridge->msg.msg, line_size);

  return js_undefined(env);
}

static napi_value ten_nodejs_audio_frame_is_eof(napi_env env,
                                                napi_callback_info info) {
  const size_t argc = 1;
  napi_value args[argc];  // this

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  bool is_eof = ten_audio_frame_is_eof(audio_frame_bridge->msg.msg);

  napi_value js_is_eof = NULL;
  status = napi_get_boolean(env, is_eof, &js_is_eof);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to create is_eof.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to create is_eof: %d", status);
  }

  return js_is_eof;
}

static napi_value ten_nodejs_audio_frame_set_eof(napi_env env,
                                                 napi_callback_info info) {
  const size_t argc = 2;
  napi_value args[argc];  // this, is_eof

  if (!ten_nodejs_get_js_func_args(env, info, args, argc)) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Incorrect number of parameters passed.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Should not happen.");
  }

  ten_nodejs_audio_frame_t *audio_frame_bridge = NULL;
  napi_status status = napi_unwrap(env, args[0], (void **)&audio_frame_bridge);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH,
                     "Failed to unwrap JS audio_frame object.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to unwrap JS audio_frame object: %d", status);
  }

  bool is_eof = false;
  status = napi_get_value_bool(env, args[1], &is_eof);
  if (status != napi_ok) {
    napi_fatal_error(NULL, NAPI_AUTO_LENGTH, "Failed to get is_eof.",
                     NAPI_AUTO_LENGTH);
    TEN_ASSERT(0, "Failed to get is_eof: %d", status);
  }

  ten_audio_frame_set_eof(audio_frame_bridge->msg.msg, is_eof);

  return js_undefined(env);
}

napi_value ten_nodejs_audio_frame_wrap(napi_env env,
                                       ten_shared_ptr_t *audio_frame) {
  TEN_ASSERT(audio_frame && ten_msg_check_integrity(audio_frame),
             "Should not happen.");

  ten_nodejs_audio_frame_t *audio_frame_bridge =
      TEN_MALLOC(sizeof(ten_nodejs_audio_frame_t));
  TEN_ASSERT(audio_frame_bridge, "Failed to allocate memory.");

  ten_nodejs_msg_init_from_c_msg(&audio_frame_bridge->msg, audio_frame);

  napi_value js_msg_name = NULL;
  napi_value js_create_shell_only_flag = NULL;

  const char *msg_name = ten_msg_get_name(audio_frame);
  TEN_ASSERT(msg_name, "Should not happen.");

  napi_status status =
      napi_create_string_utf8(env, msg_name, NAPI_AUTO_LENGTH, &js_msg_name);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_msg_name != NULL,
                      "Failed to create JS string: %d", status);

  status = napi_get_boolean(env, true, &js_create_shell_only_flag);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && js_create_shell_only_flag != NULL,
                      "Failed to create JS boolean: %d", status);

  napi_value argv[] = {js_msg_name, js_create_shell_only_flag};

  napi_value js_audio_frame = ten_nodejs_create_new_js_object_and_wrap(
      env, js_audio_frame_constructor_ref, audio_frame_bridge,
      ten_nodejs_audio_frame_finalize,
      &audio_frame_bridge->msg.bridge.js_instance_ref, 2, argv);
  ASSERT_IF_NAPI_FAIL(js_audio_frame != NULL,
                      "Failed to create JS audio_frame object.");

  return js_audio_frame;
}

napi_value ten_nodejs_audio_frame_module_init(napi_env env,
                                              napi_value exports) {
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_register_class);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_create);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_alloc_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_lock_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_unlock_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_buf);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_timestamp);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_timestamp);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_sample_rate);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_sample_rate);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_samples_per_channel);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_samples_per_channel);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_bytes_per_sample);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_bytes_per_sample);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_number_of_channels);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_number_of_channels);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_data_fmt);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_data_fmt);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_get_line_size);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_line_size);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_is_eof);
  EXPORT_FUNC(env, exports, ten_nodejs_audio_frame_set_eof);

  return exports;
}
