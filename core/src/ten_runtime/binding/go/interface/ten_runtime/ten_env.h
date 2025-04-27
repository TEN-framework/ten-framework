//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

#define TEN_GO_NO_RESPONSE_HANDLER 0
#define TEN_GO_TEN_ENV_SIGNATURE 0xCCCC1DD4BB4CA743U

typedef struct ten_env_t ten_env_t;
typedef struct ten_go_extension_t ten_go_extension_t;
typedef struct ten_go_metadata_t ten_go_metadata_t;
typedef struct ten_go_msg_t ten_go_msg_t;
typedef struct ten_go_ten_env_t ten_go_ten_env_t;
typedef struct ten_go_value_t ten_go_value_t;

void ten_go_ten_env_on_configure_done(uintptr_t bridge_addr);

void ten_go_ten_env_on_init_done(uintptr_t bridge_addr);

void ten_go_ten_env_on_start_done(uintptr_t bridge_addr);

void ten_go_ten_env_on_stop_done(uintptr_t bridge_addr);

void ten_go_ten_env_on_deinit_done(uintptr_t bridge_addr);

void ten_go_ten_env_on_create_instance_done(uintptr_t bridge_addr,
                                            uintptr_t instance_bridge_addr,
                                            uintptr_t context_addr);

ten_go_error_t ten_go_ten_env_return_result(uintptr_t bridge_addr,
                                            uintptr_t cmd_result_bridge_addr,
                                            ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_send_cmd(uintptr_t bridge_addr,
                                       uintptr_t cmd_bridge_addr,
                                       ten_go_handle_t handler_id, bool is_ex);

ten_go_error_t ten_go_ten_env_send_data(uintptr_t bridge_addr,
                                        uintptr_t data_bridge_addr,
                                        ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_send_video_frame(
    uintptr_t bridge_addr, uintptr_t video_frame_bridge_addr,
    ten_go_handle_t handler_id);

ten_go_error_t ten_go_ten_env_send_audio_frame(
    uintptr_t bridge_addr, uintptr_t audio_frame_bridge_addr,
    ten_go_handle_t handler_id);

void ten_go_ten_env_finalize(uintptr_t bridge_addr);

const char *ten_go_ten_env_debug_info(uintptr_t bridge_addr);

// In general, there are two ways to get a property from a ten_env_t instance.
//
// - One is getting property based on an explicit type, ex:
//   ten_go_ten_property_get_int8. But if the type is string or buffer, the
//   property size must be known first, in other words, we need to fall back to
//   the second way even through we know the property type. That's why we do not
//   provide a function such as `ten_go_ten_property_get_string`.
//
// - Another is the property type is unknown, or the property size must be known
//   first. In this case, the `ten_go_ten_property_get_type_and_size` function
//   will be called first. And as the ten_value_t is cloned during
//   `ten_go_ten_property_get_type_and_size`, so next we can get the property
//   from the preflight ten_value_t.

/**
 * @brief Get the type and size of a property of the ten_env_t. This function is
 * called before getting property from the GO side, as the GO side needs to do
 * some preparation if the property is not a primitive type. Ex: the memory
 * should be allocated from the GO side, if the property is a buffer.
 *
 * @param path The underlying buffer of the GO string, which is passed with
 * unsafe.Pointer in GO world, so the type of @a path is void*, not char*. Only
 * the read operation is permitted. And the buffer is managed by GO, do not read
 * it after the blocking cgo call.
 *
 * @param path_len The length of the underlying buffer.
 *
 * @param type The output for the type of the property.
 *
 * @param size The output for the size of the property. The size is only used if
 * the property is not a primitive type.
 *
 * @param value_addr The output for the ten_value_t* of the property.
 */
ten_go_error_t ten_go_ten_env_get_property_type_and_size(
    uintptr_t bridge_addr, const void *path, int path_len, uint8_t *type,
    uintptr_t *size, uintptr_t *value_addr, uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_bool(uintptr_t bridge_addr,
                                                const void *path, int path_len,
                                                bool value,
                                                uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_int8(uintptr_t bridge_addr,
                                                const void *path, int path_len,
                                                int8_t value,
                                                uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_int16(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int16_t value,
                                                 uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_int32(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int32_t value,
                                                 uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_int64(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 int64_t value,
                                                 uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_uint8(uintptr_t bridge_addr,
                                                 const void *path, int path_len,
                                                 uint8_t value,
                                                 uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_uint16(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint16_t value,
                                                  uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_uint32(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint32_t value,
                                                  uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_uint64(uintptr_t bridge_addr,
                                                  const void *path,
                                                  int path_len, uint64_t value,
                                                  uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_float32(uintptr_t bridge_addr,
                                                   const void *path,
                                                   int path_len, float value,
                                                   uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_float64(uintptr_t bridge_addr,
                                                   const void *path,
                                                   int path_len, double value,
                                                   uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_string(
    uintptr_t bridge_addr, const void *path, int path_len, const void *value,
    int value_len, uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_buf(uintptr_t bridge_addr,
                                               const void *path, int path_len,
                                               void *value, int value_len,
                                               uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_set_property_ptr(uintptr_t bridge_addr,
                                               const void *path, int path_len,
                                               ten_go_handle_t value,
                                               uintptr_t callback_handle);

/**
 * @brief Parse @a json_str as a json object and set it as a property.
 *
 * @param json_str The pointer to the underlying array of the GO slice. The data
 * it points to must be a valid json data.
 * @param json_str_len The length of the underlying array of the GO slice.
 */
ten_go_error_t ten_go_ten_env_set_property_json_bytes(
    uintptr_t bridge_addr, const void *path, int path_len, const void *json_str,
    int json_str_len, uintptr_t callback_handle);

ten_go_error_t ten_go_ten_env_init_property_from_json_bytes(
    uintptr_t bridge_addr, const void *json_str, int json_str_len);

ten_go_error_t ten_go_ten_env_log(uintptr_t bridge_addr, int level,
                                  const void *func_name, int func_name_len,
                                  const void *file_name, int file_name_len,
                                  int line_no, const void *msg, int msg_len);
