//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "ten_utils/container/list_node.h"

TEN_UTILS_API void ten_list_push_str_back(ten_list_t *self, const char *str);

TEN_UTILS_API void ten_list_push_str_front(ten_list_t *self, const char *str);

TEN_UTILS_API void ten_list_push_str_with_size_back(ten_list_t *self,
                                                    const char *str,
                                                    size_t size);

TEN_UTILS_API ten_listnode_t *ten_list_find_string(ten_list_t *self,
                                                   const char *str);
