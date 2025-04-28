//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "include_internal/ten_runtime/test/env_tester.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"

typedef struct ten_go_ten_env_tester_t {
  ten_signature_t signature;

  ten_go_bridge_t bridge;

  ten_env_tester_t *c_ten_env_tester;
} ten_go_ten_env_tester_t;

TEN_RUNTIME_PRIVATE_API bool ten_go_ten_env_tester_check_integrity(
    ten_go_ten_env_tester_t *self);

TEN_RUNTIME_PRIVATE_API ten_go_ten_env_tester_t *
ten_go_ten_env_tester_reinterpret(uintptr_t bridge_addr);

TEN_RUNTIME_PRIVATE_API ten_go_ten_env_tester_t *ten_go_ten_env_tester_wrap(
    ten_env_tester_t *c_ten_env_tester);

TEN_RUNTIME_PRIVATE_API ten_go_handle_t
ten_go_ten_env_tester_go_handle(ten_go_ten_env_tester_t *self);

TEN_RUNTIME_API ten_go_handle_t tenGoCreateTenEnvTester(uintptr_t);
