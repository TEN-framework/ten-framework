//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/signature.h"

#define TEN_ENV_TESTER_PROXY_SIGNATURE 0x12D37E14C7045A41U

typedef struct ten_env_tester_t ten_env_tester_t;
typedef struct ten_env_proxy_t ten_env_proxy_t;

typedef struct ten_env_tester_proxy_t {
  ten_signature_t signature;

  ten_env_tester_t *ten_env_tester;
  ten_env_proxy_t *ten_env_proxy;
} ten_env_tester_proxy_t;
