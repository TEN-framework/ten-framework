//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg_conversion/msg_and_its_result_conversion.h"

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

ten_msg_and_its_result_conversion_t *ten_msg_and_its_result_conversion_create(
    ten_shared_ptr_t *msg, ten_msg_conversion_t *result_conversion) {
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  ten_msg_and_its_result_conversion_t *self =
      (ten_msg_and_its_result_conversion_t *)TEN_MALLOC(
          sizeof(ten_msg_and_its_result_conversion_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->msg = ten_shared_ptr_clone(msg);

  self->result_conversion = result_conversion;

  return self;
}

void ten_msg_and_its_result_conversion_destroy(
    ten_msg_and_its_result_conversion_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (self->msg) {
    ten_shared_ptr_destroy(self->msg);
    self->msg = NULL;
  }

  TEN_FREE(self);
}
