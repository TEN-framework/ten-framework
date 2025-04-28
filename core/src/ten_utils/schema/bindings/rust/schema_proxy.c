//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/schema/bindings/rust/schema_proxy.h"

#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"

ten_schema_t *ten_schema_create_from_json_str_proxy(const char *json_string,
                                                    const char **err_msg) {
  TEN_ASSERT(json_string, "Invalid argument.");
  return ten_schema_create_from_json_str(json_string, err_msg);
}

void ten_schema_destroy_proxy(const ten_schema_t *self) {
  ten_schema_t *self_ = (ten_schema_t *)self;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self_), "Invalid argument.");

  ten_schema_destroy(self_);
}

bool ten_schema_adjust_and_validate_json_str_proxy(const ten_schema_t *self,
                                                   const char *json_string,
                                                   const char **err_msg) {
  ten_schema_t *self_ = (ten_schema_t *)self;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self_), "Invalid argument.");
  TEN_ASSERT(json_string, "Invalid argument.");

  return ten_schema_adjust_and_validate_json_str(self_, json_string, err_msg);
}

bool ten_schema_is_compatible_proxy(const ten_schema_t *self,
                                    const ten_schema_t *target,
                                    const char **err_msg) {
  ten_schema_t *self_ = (ten_schema_t *)self;
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_schema_check_integrity(self_), "Invalid argument.");

  ten_schema_t *target_ = (ten_schema_t *)target;
  TEN_ASSERT(target && ten_schema_check_integrity(target_),
             "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool result = ten_schema_is_compatible(self_, target_, &err);
  if (!result) {
    *err_msg = ten_strdup(ten_error_message(&err));
  }

  ten_error_deinit(&err);
  return result;
}
