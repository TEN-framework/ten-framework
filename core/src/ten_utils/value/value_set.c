//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/value/value_set.h"

#include "ten_utils/container/list.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"

bool ten_value_set_int64(ten_value_t *self, int64_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_INT64, "Invalid argument.");

  self->content.int64 = value;

  return true;
}

bool ten_value_set_int32(ten_value_t *self, int32_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_INT32, "Invalid argument.");

  self->content.int32 = value;

  return true;
}

bool ten_value_set_int16(ten_value_t *self, int16_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_INT16, "Invalid argument.");

  self->content.int16 = value;

  return true;
}

bool ten_value_set_int8(ten_value_t *self, int8_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_INT8, "Invalid argument.");

  self->content.int8 = value;

  return true;
}

bool ten_value_set_uint64(ten_value_t *self, uint64_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_UINT64, "Invalid argument.");

  self->content.uint64 = value;

  return true;
}

bool ten_value_set_uint32(ten_value_t *self, uint32_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_UINT32, "Invalid argument.");

  self->content.uint32 = value;

  return true;
}

bool ten_value_set_uint16(ten_value_t *self, uint16_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_UINT16, "Invalid argument.");

  self->content.uint16 = value;

  return true;
}

bool ten_value_set_uint8(ten_value_t *self, uint8_t value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_UINT8, "Invalid argument.");

  self->content.uint8 = value;

  return true;
}

bool ten_value_set_bool(ten_value_t *self, bool value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_BOOL, "Invalid argument.");

  self->content.boolean = value;

  return true;
}

bool ten_value_set_float32(ten_value_t *self, float value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_FLOAT32, "Invalid argument.");

  self->content.float32 = value;

  return true;
}

bool ten_value_set_float64(ten_value_t *self, double value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_FLOAT64, "Invalid argument.");

  self->content.float64 = value;

  return true;
}

bool ten_value_set_string(ten_value_t *self, const char *str) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_is_string(self), "Invalid argument.");

  ten_string_set_formatted(&self->content.string, "%.*s", strlen(str), str);

  return true;
}

bool ten_value_set_string_with_size(ten_value_t *self, const char *str,
                                    size_t len) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(ten_value_is_string(self), "Invalid argument.");
  TEN_ASSERT(str, "Invalid argument.");

  ten_string_set_formatted(&self->content.string, "%.*s", len, str);

  return true;
}

bool ten_value_set_array_with_move(ten_value_t *self, ten_list_t *value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_ARRAY, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_list_clear(&self->content.array);
  ten_list_swap(&self->content.array, value);

  return true;
}

bool ten_value_set_object_with_move(ten_value_t *self, ten_list_t *value) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_OBJECT, "Invalid argument.");
  TEN_ASSERT(value, "Invalid argument.");

  ten_list_clear(&self->content.object);
  ten_list_swap(&self->content.object, value);

  return true;
}
