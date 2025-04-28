//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/error.h"

#include <stdarg.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_error_check_integrity(ten_error_t *self) {
  TEN_ASSERT(self, "Invalid argument");

  if (ten_signature_get(&self->signature) != TEN_ERROR_SIGNATURE) {
    return false;
  }
  return true;
}

void ten_error_init(ten_error_t *self) {
  TEN_ASSERT(self, "Invalid argument");

  ten_signature_set(&self->signature, TEN_ERROR_SIGNATURE);

  self->error_code = TEN_ERROR_CODE_OK;
  TEN_STRING_INIT(self->error_message);
}

void ten_error_deinit(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  ten_string_deinit(&self->error_message);
}

ten_error_t *ten_error_create(void) {
  ten_error_t *self = (ten_error_t *)TEN_MALLOC(sizeof(ten_error_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_error_init(self);

  return self;
}

void ten_error_copy(ten_error_t *self, ten_error_t *other) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");
  TEN_ASSERT(other && ten_error_check_integrity(other), "Invalid argument");

  self->error_code = other->error_code;
  ten_string_copy(&self->error_message, &other->error_message);
}

bool ten_error_prepend_error_message(ten_error_t *self, const char *fmt, ...) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  va_list ap;
  va_start(ap, fmt);
  ten_string_prepend_from_va_list(&self->error_message, fmt, ap);
  va_end(ap);

  return true;
}

bool ten_error_append_error_message(ten_error_t *self, const char *fmt, ...) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  va_list ap;
  va_start(ap, fmt);
  ten_string_append_from_va_list(&self->error_message, fmt, ap);
  va_end(ap);

  return true;
}

bool ten_error_set(ten_error_t *self, ten_error_code_t error_code,
                   const char *fmt, ...) {
  TEN_ASSERT(self && ten_error_check_integrity(self) && fmt,
             "Invalid argument");

  va_list ap;
  va_start(ap, fmt);
  bool result = ten_error_vset(self, error_code, fmt, ap);
  va_end(ap);

  return result;
}

bool ten_error_vset(ten_error_t *self, ten_error_code_t error_code,
                    const char *fmt, va_list ap) {
  TEN_ASSERT(self && ten_error_check_integrity(self) && fmt,
             "Invalid argument");

  self->error_code = error_code;
  ten_string_clear(&self->error_message);
  ten_string_append_from_va_list(&self->error_message, fmt, ap);

  return true;
}

ten_error_code_t ten_error_code(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");
  return self->error_code;
}

const char *ten_error_message(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  return ten_string_get_raw_str(&self->error_message);
}

void ten_error_reset(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  self->error_code = TEN_ERROR_CODE_OK;
  ten_string_clear(&self->error_message);
}

void ten_error_destroy(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  ten_error_deinit(self);
  TEN_FREE(self);
}

bool ten_error_is_success(ten_error_t *self) {
  TEN_ASSERT(self && ten_error_check_integrity(self), "Invalid argument");

  return self->error_code == TEN_ERROR_CODE_OK;
}
