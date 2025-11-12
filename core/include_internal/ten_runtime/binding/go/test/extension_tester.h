//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "src/ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_utils/lib/signature.h"

#define TEN_GO_EXTENSION_TESTER_SIGNATURE 0xF542240B13C47F46U

typedef struct ten_go_extension_tester_t {
  ten_signature_t signature;

  ten_go_bridge_t bridge;

  ten_extension_tester_t *c_extension_tester;
} ten_go_extension_tester_t;

TEN_RUNTIME_PRIVATE_API bool ten_go_extension_tester_check_integrity(
    ten_go_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_extension_tester_t *
ten_go_extension_tester_reinterpret(uintptr_t bridge_addr);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_extension_tester_go_handle(ten_go_extension_tester_t *self);

TEN_RUNTIME_API ten_go_extension_tester_t *
ten_go_extension_tester_create_internal(ten_go_handle_t go_extension_tester);

TEN_RUNTIME_PRIVATE_API ten_extension_tester_t *
ten_go_extension_tester_c_extension_tester(ten_go_extension_tester_t *self);

TEN_RUNTIME_PRIVATE_API void ten_go_extension_tester_set_go_handle(
    ten_go_extension_tester_t *self, ten_go_handle_t go_handle);
