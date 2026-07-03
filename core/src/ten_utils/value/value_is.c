//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

bool ten_value_is_object(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_OBJECT) ? true : false;
}

bool ten_value_is_array(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_ARRAY) ? true : false;
}

bool ten_value_is_string(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_STRING) ? true : false;
}

bool ten_value_is_invalid(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_INVALID) ? true : false;
}

bool ten_value_is_int8(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_INT8) ? true : false;
}

bool ten_value_is_int16(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_INT16) ? true : false;
}

bool ten_value_is_int32(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_INT32) ? true : false;
}

bool ten_value_is_int64(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_INT64) ? true : false;
}

bool ten_value_is_uint8(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_UINT8) ? true : false;
}

bool ten_value_is_uint16(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_UINT16) ? true : false;
}

bool ten_value_is_uint32(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_UINT32) ? true : false;
}

bool ten_value_is_uint64(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_UINT64) ? true : false;
}

bool ten_value_is_float32(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_FLOAT32) ? true : false;
}

bool ten_value_is_float64(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_FLOAT64) ? true : false;
}

bool ten_value_is_number(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_int32(self) || ten_value_is_int64(self) ||
      ten_value_is_float32(self) || ten_value_is_float64(self)) {
    return true;
  }
  return false;
}

bool ten_value_is_null(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_NULL) ? true : false;
}

bool ten_value_is_bool(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_BOOL) ? true : false;
}

bool ten_value_is_ptr(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_PTR) ? true : false;
}

bool ten_value_is_buf(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return (self->type == TEN_TYPE_BUF) ? true : false;
}
