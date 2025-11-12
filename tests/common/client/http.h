//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/string.h"

TEN_RUNTIME_PRIVATE_API void ten_test_http_client_init(void);

TEN_RUNTIME_PRIVATE_API void ten_test_http_client_deinit(void);

TEN_RUNTIME_PRIVATE_API void ten_test_http_client_get(const char *url,
                                                      ten_string_t *result);

TEN_RUNTIME_PRIVATE_API void ten_test_http_client_post(const char *url,
                                                       const char *body,
                                                       ten_string_t *result);
