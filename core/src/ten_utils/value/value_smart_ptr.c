//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/value/value_smart_ptr.h"

#include <stdlib.h>

#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"

bool ten_value_construct_for_smart_ptr(ten_value_t *self,
                                       TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_PTR, "Invalid argument.");
  TEN_ASSERT(self->content.ptr, "Invalid argument.");

  return true;
}

bool ten_value_copy_for_smart_ptr(ten_value_t *dest, ten_value_t *src,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest, "Invalid argument.");
  TEN_ASSERT(src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_PTR, "Invalid argument.");
  TEN_ASSERT(src->content.ptr, "Invalid argument.");

  TEN_LOGD("Copy c_value %p -> %p", src, dest);

  ten_value_reset_to_ptr(dest, ten_smart_ptr_clone(src->content.ptr),
                         src->construct, src->copy, src->destruct);

  return true;
}

bool ten_value_destruct_for_smart_ptr(ten_value_t *self,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_PTR, "Invalid argument.");
  TEN_ASSERT(self->content.ptr, "Invalid argument.");

  TEN_LOGD("Delete c_value %p", self);

  ten_smart_ptr_destroy(self->content.ptr);
  self->content.ptr = NULL;

  return true;
}
