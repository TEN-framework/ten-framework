//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stddef.h>

#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_data_t ten_data_t;

TEN_RUNTIME_API ten_shared_ptr_t *ten_data_create(const char *name,
                                                  ten_error_t *err);

TEN_RUNTIME_API ten_buf_t *ten_data_peek_buf(ten_shared_ptr_t *self);

TEN_RUNTIME_API void ten_data_set_buf_with_move(ten_shared_ptr_t *self,
                                                ten_buf_t *buf);

TEN_RUNTIME_API uint8_t *ten_data_alloc_buf(ten_shared_ptr_t *self,
                                            size_t size);
