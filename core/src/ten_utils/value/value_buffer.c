//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/value/value_buffer.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_kv.h"

#define TEN_VALUE_BUFFER_VERSION 1

// Helper macros for buffer operations
#define WRITE_TO_BUFFER(buf, pos, size, data, data_size) \
  do {                                                   \
    if ((pos) + (data_size) > (size)) return false;      \
    memcpy((buf) + (pos), (data), (data_size));          \
    (pos) += (data_size);                                \
  } while (0)

#define READ_FROM_BUFFER(buf, pos, size, data, data_size) \
  do {                                                    \
    if ((pos) + (data_size) > (size)) return false;       \
    memcpy((data), (buf) + (pos), (data_size));           \
    (pos) += (data_size);                                 \
  } while (0)

// Convert TEN_TYPE to buffer type
static TEN_VALUE_BUFFER_TYPE ten_type_to_buffer_type(TEN_TYPE type) {
  switch (type) {
  case TEN_TYPE_INVALID:
    return TEN_VALUE_BUFFER_TYPE_INVALID;
  case TEN_TYPE_BOOL:
    return TEN_VALUE_BUFFER_TYPE_BOOL;
  case TEN_TYPE_INT8:
    return TEN_VALUE_BUFFER_TYPE_INT8;
  case TEN_TYPE_INT16:
    return TEN_VALUE_BUFFER_TYPE_INT16;
  case TEN_TYPE_INT32:
    return TEN_VALUE_BUFFER_TYPE_INT32;
  case TEN_TYPE_INT64:
    return TEN_VALUE_BUFFER_TYPE_INT64;
  case TEN_TYPE_UINT8:
    return TEN_VALUE_BUFFER_TYPE_UINT8;
  case TEN_TYPE_UINT16:
    return TEN_VALUE_BUFFER_TYPE_UINT16;
  case TEN_TYPE_UINT32:
    return TEN_VALUE_BUFFER_TYPE_UINT32;
  case TEN_TYPE_UINT64:
    return TEN_VALUE_BUFFER_TYPE_UINT64;
  case TEN_TYPE_FLOAT32:
    return TEN_VALUE_BUFFER_TYPE_FLOAT32;
  case TEN_TYPE_FLOAT64:
    return TEN_VALUE_BUFFER_TYPE_FLOAT64;
  case TEN_TYPE_STRING:
    return TEN_VALUE_BUFFER_TYPE_STRING;
  case TEN_TYPE_BUF:
    return TEN_VALUE_BUFFER_TYPE_BUF;
  case TEN_TYPE_ARRAY:
    return TEN_VALUE_BUFFER_TYPE_ARRAY;
  case TEN_TYPE_OBJECT:
    return TEN_VALUE_BUFFER_TYPE_OBJECT;
  case TEN_TYPE_PTR:
    return TEN_VALUE_BUFFER_TYPE_PTR;
  default:
    return TEN_VALUE_BUFFER_TYPE_INVALID;
  }
}

// Convert buffer type to TEN_TYPE
static TEN_TYPE buffer_type_to_ten_type(TEN_VALUE_BUFFER_TYPE type) {
  switch (type) {
  case TEN_VALUE_BUFFER_TYPE_INVALID:
    return TEN_TYPE_INVALID;
  case TEN_VALUE_BUFFER_TYPE_BOOL:
    return TEN_TYPE_BOOL;
  case TEN_VALUE_BUFFER_TYPE_INT8:
    return TEN_TYPE_INT8;
  case TEN_VALUE_BUFFER_TYPE_INT16:
    return TEN_TYPE_INT16;
  case TEN_VALUE_BUFFER_TYPE_INT32:
    return TEN_TYPE_INT32;
  case TEN_VALUE_BUFFER_TYPE_INT64:
    return TEN_TYPE_INT64;
  case TEN_VALUE_BUFFER_TYPE_UINT8:
    return TEN_TYPE_UINT8;
  case TEN_VALUE_BUFFER_TYPE_UINT16:
    return TEN_TYPE_UINT16;
  case TEN_VALUE_BUFFER_TYPE_UINT32:
    return TEN_TYPE_UINT32;
  case TEN_VALUE_BUFFER_TYPE_UINT64:
    return TEN_TYPE_UINT64;
  case TEN_VALUE_BUFFER_TYPE_FLOAT32:
    return TEN_TYPE_FLOAT32;
  case TEN_VALUE_BUFFER_TYPE_FLOAT64:
    return TEN_TYPE_FLOAT64;
  case TEN_VALUE_BUFFER_TYPE_STRING:
    return TEN_TYPE_STRING;
  case TEN_VALUE_BUFFER_TYPE_BUF:
    return TEN_TYPE_BUF;
  case TEN_VALUE_BUFFER_TYPE_ARRAY:
    return TEN_TYPE_ARRAY;
  case TEN_VALUE_BUFFER_TYPE_OBJECT:
    return TEN_TYPE_OBJECT;
  case TEN_VALUE_BUFFER_TYPE_PTR:
    return TEN_TYPE_PTR;
  case TEN_VALUE_BUFFER_TYPE_JSON_STRING:
    return TEN_TYPE_STRING;
  default:
    return TEN_TYPE_INVALID;
  }
}

// Forward declarations for internal functions
static size_t calculate_value_size(ten_value_t *value);
static bool serialize_value_content(ten_value_t *value, uint8_t *buffer,
                                    size_t buffer_size, size_t *pos);
static ten_value_t *deserialize_value_content(const uint8_t *buffer,
                                              size_t buffer_size, size_t *pos,
                                              TEN_VALUE_BUFFER_TYPE type);

// Calculate the buffer size needed to serialize a value.
static size_t ten_value_calculate_serialize_size(ten_value_t *value,
                                                 ten_error_t *err) {
  TEN_ASSERT(value, "Invalid argument.");

  if (!ten_value_check_integrity(value)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value integrity");
    }
    return 0;
  }

  size_t total_size = sizeof(ten_value_buffer_header_t);
  total_size += calculate_value_size(value);

  return total_size;
}

static size_t calculate_value_size(ten_value_t *value) {
  TEN_ASSERT(value, "Invalid argument.");

  TEN_TYPE type = ten_value_get_type(value);

  switch (type) {
  case TEN_TYPE_INVALID:
    return 0;

  case TEN_TYPE_BOOL:
  case TEN_TYPE_INT8:
  case TEN_TYPE_UINT8:
    return sizeof(uint8_t);

  case TEN_TYPE_INT16:
  case TEN_TYPE_UINT16:
    return sizeof(uint16_t);

  case TEN_TYPE_INT32:
  case TEN_TYPE_UINT32:
    return sizeof(uint32_t);

  case TEN_TYPE_INT64:
  case TEN_TYPE_UINT64:
    return sizeof(uint64_t);

  case TEN_TYPE_FLOAT32:
    return sizeof(float);

  case TEN_TYPE_FLOAT64:
    return sizeof(double);

  case TEN_TYPE_STRING: {
    ten_string_t *str = ten_value_peek_string(value);
    if (!str) {
      // Just length field
      return sizeof(uint32_t);
    }
    size_t str_len = ten_string_len(str);
    return sizeof(uint32_t) + str_len;
  }

  case TEN_TYPE_BUF: {
    ten_buf_t *buf = ten_value_peek_buf(value, NULL);
    if (!buf) {
      // Just length field
      return sizeof(uint32_t);
    }
    size_t buf_size = ten_buf_get_size(buf);
    return sizeof(uint32_t) + buf_size;
  }

  case TEN_TYPE_ARRAY: {
    size_t total_size = sizeof(uint32_t);  // Array length

    ten_value_array_foreach(value, iter) {
      ten_value_t *item = ten_ptr_listnode_get(iter.node);
      total_size += sizeof(uint8_t);  // Item type
      total_size += calculate_value_size(item);
    }

    return total_size;
  }

  case TEN_TYPE_OBJECT: {
    size_t total_size = sizeof(uint32_t);  // Object size

    ten_value_object_foreach(value, iter) {
      ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
      ten_string_t *key = ten_value_kv_get_key(kv);
      ten_value_t *val = ten_value_kv_get_value(kv);

      size_t key_len = ten_string_len(key);
      total_size += sizeof(uint32_t) + key_len;  // Key length + key data
      total_size += sizeof(uint8_t);             // Value type
      total_size += calculate_value_size(val);
    }

    return total_size;
  }

  default:
    return 0;
  }
}

// Serialize a ten_value_t into a buffer.
//
// This function serializes a ten_value_t structure into a compact binary
// format that can be efficiently transferred across language boundaries.
static bool ten_value_serialize_to_buffer(ten_value_t *value, uint8_t *buffer,
                                          size_t buffer_size,
                                          size_t *bytes_written,
                                          ten_error_t *err) {
  TEN_ASSERT(value && buffer && bytes_written, "Invalid argument.");

  if (!ten_value_check_integrity(value)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value integrity");
    }
    return false;
  }

  size_t required_size = ten_value_calculate_serialize_size(value, err);
  if (required_size == 0 || required_size > buffer_size) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Buffer too small");
    }
    return false;
  }

  size_t pos = 0;

  // Write header
  ten_value_buffer_header_t header;
  header.magic = TEN_VALUE_BUFFER_MAGIC;
  header.version = TEN_VALUE_BUFFER_VERSION;
  header.type = ten_type_to_buffer_type(ten_value_get_type(value));
  header.size = (uint32_t)(required_size - sizeof(ten_value_buffer_header_t));

  WRITE_TO_BUFFER(buffer, pos, buffer_size, &header, sizeof(header));

  // Write value content
  if (!serialize_value_content(value, buffer, buffer_size, &pos)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to serialize value content");
    }
    return false;
  }

  *bytes_written = pos;
  return true;
}

static bool serialize_value_content(ten_value_t *value, uint8_t *buffer,
                                    size_t buffer_size, size_t *pos) {
  TEN_ASSERT(value && buffer && pos, "Invalid argument.");

  TEN_TYPE type = ten_value_get_type(value);

  switch (type) {
  case TEN_TYPE_INVALID:
    // No additional data to write
    break;

  case TEN_TYPE_BOOL: {
    uint8_t val = ten_value_get_bool(value, NULL) ? 1 : 0;
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_INT8: {
    int8_t val = ten_value_get_int8(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_INT16: {
    int16_t val = ten_value_get_int16(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_INT32: {
    int32_t val = ten_value_get_int32(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_INT64: {
    int64_t val = ten_value_get_int64(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_UINT8: {
    uint8_t val = ten_value_get_uint8(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_UINT16: {
    uint16_t val = ten_value_get_uint16(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_UINT32: {
    uint32_t val = ten_value_get_uint32(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_UINT64: {
    uint64_t val = ten_value_get_uint64(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_FLOAT32: {
    float val = ten_value_get_float32(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_FLOAT64: {
    double val = ten_value_get_float64(value, NULL);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    break;
  }

  case TEN_TYPE_STRING: {
    ten_string_t *str = ten_value_peek_string(value);
    if (str) {
      const char *raw_str = ten_string_get_raw_str(str);
      uint32_t str_len = (uint32_t)ten_string_len(str);

      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &str_len, sizeof(str_len));
      if (str_len > 0) {
        WRITE_TO_BUFFER(buffer, *pos, buffer_size, raw_str, str_len);
      }
    } else {
      uint32_t str_len = 0;
      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &str_len, sizeof(str_len));
    }
    break;
  }

  case TEN_TYPE_BUF: {
    ten_buf_t *buf = ten_value_peek_buf(value, NULL);
    if (buf) {
      uint32_t buf_size = (uint32_t)ten_buf_get_size(buf);
      const uint8_t *buf_data = ten_buf_get_data(buf);

      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &buf_size, sizeof(buf_size));
      if (buf_size > 0) {
        WRITE_TO_BUFFER(buffer, *pos, buffer_size, buf_data, buf_size);
      }
    } else {
      uint32_t buf_size = 0;
      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &buf_size, sizeof(buf_size));
    }
    break;
  }

  case TEN_TYPE_ARRAY: {
    uint32_t array_len = (uint32_t)ten_value_array_size(value);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &array_len, sizeof(array_len));

    ten_value_array_foreach(value, iter) {
      ten_value_t *item = ten_ptr_listnode_get(iter.node);
      uint8_t item_type = ten_type_to_buffer_type(ten_value_get_type(item));

      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &item_type, sizeof(item_type));

      if (!serialize_value_content(item, buffer, buffer_size, pos)) {
        return false;
      }
    }
    break;
  }

  case TEN_TYPE_OBJECT: {
    uint32_t obj_size = (uint32_t)ten_list_size(&value->content.object);
    WRITE_TO_BUFFER(buffer, *pos, buffer_size, &obj_size, sizeof(obj_size));

    ten_value_object_foreach(value, iter) {
      ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
      ten_string_t *key = ten_value_kv_get_key(kv);
      ten_value_t *val = ten_value_kv_get_value(kv);

      // Write key
      const char *key_str = ten_string_get_raw_str(key);
      uint32_t key_len = (uint32_t)ten_string_len(key);
      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &key_len, sizeof(key_len));
      WRITE_TO_BUFFER(buffer, *pos, buffer_size, key_str, key_len);

      // Write value type and content
      uint8_t val_type = ten_type_to_buffer_type(ten_value_get_type(val));
      WRITE_TO_BUFFER(buffer, *pos, buffer_size, &val_type, sizeof(val_type));

      if (!serialize_value_content(val, buffer, buffer_size, pos)) {
        return false;
      }
    }
    break;
  }

  default:
    return false;
  }

  return true;
}

// Validate buffer format and extract basic information.
static bool ten_value_buffer_validate_header(const uint8_t *buffer,
                                             size_t buffer_size,
                                             ten_value_buffer_header_t *header,
                                             ten_error_t *err) {
  TEN_ASSERT(buffer && header, "Invalid argument.");

  if (buffer_size < sizeof(ten_value_buffer_header_t)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Buffer too small for header");
    }
    return false;
  }

  memcpy(header, buffer, sizeof(ten_value_buffer_header_t));

  if (header->magic != TEN_VALUE_BUFFER_MAGIC) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid buffer magic number");
    }
    return false;
  }

  if (header->version != TEN_VALUE_BUFFER_VERSION) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Unsupported buffer version");
    }
    return false;
  }

  if (buffer_type_to_ten_type((TEN_VALUE_BUFFER_TYPE)header->type) ==
      TEN_TYPE_INVALID) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid buffer type");
    }
    return false;
  }

  if (header->size + sizeof(ten_value_buffer_header_t) > buffer_size) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Buffer size mismatch");
    }
    return false;
  }

  return true;
}

// Deserialize a ten_value_t from a buffer.
//
// This function reconstructs a ten_value_t structure from binary data
// created by ten_value_serialize_to_buffer.
static ten_value_t *ten_value_deserialize_from_buffer(const uint8_t *buffer,
                                                      size_t buffer_size,
                                                      size_t *bytes_consumed,
                                                      ten_error_t *err) {
  TEN_ASSERT(buffer && bytes_consumed, "Invalid argument.");

  ten_value_buffer_header_t header;
  if (!ten_value_buffer_validate_header(buffer, buffer_size, &header, err)) {
    return NULL;
  }

  size_t pos = sizeof(ten_value_buffer_header_t);
  TEN_VALUE_BUFFER_TYPE buf_type = (TEN_VALUE_BUFFER_TYPE)header.type;

  ten_value_t *value =
      deserialize_value_content(buffer, buffer_size, &pos, buf_type);
  if (!value) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to deserialize value content");
    }
    return NULL;
  }

  *bytes_consumed = pos;
  return value;
}

static ten_value_t *deserialize_value_content(const uint8_t *buffer,
                                              size_t buffer_size, size_t *pos,
                                              TEN_VALUE_BUFFER_TYPE type) {
  TEN_ASSERT(buffer && pos, "Invalid argument.");

  ten_value_t *value = NULL;

  switch (type) {
  case TEN_VALUE_BUFFER_TYPE_INVALID:
    value = ten_value_create_invalid();
    break;

  case TEN_VALUE_BUFFER_TYPE_BOOL: {
    uint8_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_bool(val != 0);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_INT8: {
    int8_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_int8(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_INT16: {
    int16_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_int16(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_INT32: {
    int32_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_int32(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_INT64: {
    int64_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_int64(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_UINT8: {
    uint8_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_uint8(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_UINT16: {
    uint16_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_uint16(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_UINT32: {
    uint32_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_uint32(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_UINT64: {
    uint64_t val = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_uint64(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_FLOAT32: {
    float val = 0.0F;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_float32(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_FLOAT64: {
    double val = 0.0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &val, sizeof(val));
    value = ten_value_create_float64(val);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_STRING: {
    uint32_t str_len = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &str_len, sizeof(str_len));

    if (str_len == 0) {
      value = ten_value_create_string("");
    } else {
      char *str_data = (char *)TEN_MALLOC(str_len + 1);
      if (!str_data) {
        return NULL;
      }

      READ_FROM_BUFFER(buffer, *pos, buffer_size, str_data, str_len);
      str_data[str_len] = '\0';

      value = ten_value_create_string_with_size(str_data, str_len);
      TEN_FREE(str_data);
    }
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_BUF: {
    uint32_t buf_size = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &buf_size, sizeof(buf_size));

    ten_buf_t buf;
    ten_buf_init_with_owned_data(&buf, buf_size);

    if (buf_size > 0) {
      uint8_t *buf_data = ten_buf_get_data(&buf);
      READ_FROM_BUFFER(buffer, *pos, buffer_size, buf_data, buf_size);
    }

    value = ten_value_create_buf_with_move(buf);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_ARRAY: {
    uint32_t array_len = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &array_len, sizeof(array_len));

    ten_list_t array;
    ten_list_init(&array);

    for (uint32_t i = 0; i < array_len; i++) {
      uint8_t item_type = 0;
      READ_FROM_BUFFER(buffer, *pos, buffer_size, &item_type,
                       sizeof(item_type));

      ten_value_t *item = deserialize_value_content(
          buffer, buffer_size, pos, (TEN_VALUE_BUFFER_TYPE)item_type);
      if (!item) {
        // Clean up and return NULL
        ten_value_array_foreach(value, iter) {
          ten_value_t *val = ten_ptr_listnode_get(iter.node);
          ten_value_destroy(val);
        }
        ten_list_reset(&array);
        return NULL;
      }

      ten_list_push_back(
          &array,
          ten_ptr_listnode_create(
              item, (ten_ptr_listnode_destroy_func_t)ten_value_destroy));
    }

    value = ten_value_create_array_with_move(&array);
    break;
  }

  case TEN_VALUE_BUFFER_TYPE_OBJECT: {
    uint32_t obj_size = 0;
    READ_FROM_BUFFER(buffer, *pos, buffer_size, &obj_size, sizeof(obj_size));

    ten_list_t object;
    ten_list_init(&object);

    for (uint32_t i = 0; i < obj_size; i++) {
      // Read key
      uint32_t key_len = 0;
      READ_FROM_BUFFER(buffer, *pos, buffer_size, &key_len, sizeof(key_len));

      char *key_data = (char *)TEN_MALLOC(key_len + 1);
      if (!key_data) {
        // Clean up and return NULL
        ten_value_object_foreach(value, iter) {
          ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
          ten_value_kv_destroy(kv);
        }
        ten_list_reset(&object);
        return NULL;
      }

      READ_FROM_BUFFER(buffer, *pos, buffer_size, key_data, key_len);
      key_data[key_len] = '\0';

      // Read value
      uint8_t val_type = 0;
      READ_FROM_BUFFER(buffer, *pos, buffer_size, &val_type, sizeof(val_type));

      ten_value_t *val = deserialize_value_content(
          buffer, buffer_size, pos, (TEN_VALUE_BUFFER_TYPE)val_type);
      if (!val) {
        TEN_FREE(key_data);
        // Clean up and return NULL
        ten_value_object_foreach(value, iter) {
          ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
          ten_value_kv_destroy(kv);
        }
        ten_list_reset(&object);
        return NULL;
      }

      ten_value_kv_t *kv = ten_value_kv_create(key_data, val);
      ten_list_push_back(
          &object,
          ten_ptr_listnode_create(
              kv, (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy));

      TEN_FREE(key_data);
    }

    value = ten_value_create_object_with_move(&object);
    break;
  }

  default:
    return NULL;
  }

  return value;
}

uint8_t *ten_value_serialize_to_buffer_c(ten_value_t *value,
                                         size_t *buffer_size,
                                         ten_error_t *err) {
  TEN_ASSERT(value && buffer_size, "Invalid argument.");

  if (!ten_value_check_integrity(value)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value integrity");
    }
    return NULL;
  }

  // Calculate required buffer size
  size_t required_size = ten_value_calculate_serialize_size(value, err);
  if (required_size == 0) {
    return NULL;
  }

  // Allocate buffer
  uint8_t *buffer = (uint8_t *)TEN_MALLOC(required_size);
  if (!buffer) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Failed to allocate buffer");
    }
    return NULL;
  }

  // Serialize to buffer
  size_t bytes_written = 0;
  bool success = ten_value_serialize_to_buffer(value, buffer, required_size,
                                               &bytes_written, err);
  if (!success) {
    TEN_FREE(buffer);
    return NULL;
  }

  *buffer_size = bytes_written;
  return buffer;
}

ten_value_t *ten_value_deserialize_from_buffer_c(const uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *bytes_consumed,
                                                 ten_error_t *err) {
  TEN_ASSERT(buffer, "Invalid argument.");

  if (buffer_size == 0) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Buffer size is zero");
    }
    return NULL;
  }

  size_t consumed = 0;
  ten_value_t *value =
      ten_value_deserialize_from_buffer(buffer, buffer_size, &consumed, err);

  if (bytes_consumed) {
    *bytes_consumed = consumed;
  }

  return value;
}
