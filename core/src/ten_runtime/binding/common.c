//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/common.h"

#include "include_internal/ten_runtime/binding/common.h"
#include "ten_utils/macro/check.h"

void ten_binding_handle_set_me_in_target_lang(ten_binding_handle_t *self,
                                              void *me_in_target_lang) {
  TEN_ASSERT(self, "Invalid argument.");
  self->me_in_target_lang = me_in_target_lang;
}

void *ten_binding_handle_get_me_in_target_lang(ten_binding_handle_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  return self->me_in_target_lang;
}
