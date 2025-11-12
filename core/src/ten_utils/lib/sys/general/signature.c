//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/signature.h"

#include "ten_utils/macro/check.h"

void ten_signature_set(ten_signature_t *signature, ten_signature_t value) {
  TEN_ASSERT(signature, "Invalid argument.");
  *signature = value;
}

ten_signature_t ten_signature_get(const ten_signature_t *signature) {
  TEN_ASSERT(signature, "Invalid argument.");
  return *signature;
}
