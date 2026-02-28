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
#include <stdint.h>
#include <string.h>

#include "ten_utils/lib/signature.h"

#define TEN_BUF_SIGNATURE 0x909BBC97B47EC291U

#define TEN_BUF_STATIC_INIT_OWNED \
  (ten_buf_t) { TEN_BUF_SIGNATURE, NULL, 0, 0, true, false }

#define TEN_BUF_STATIC_INIT_UNOWNED \
  (ten_buf_t) { TEN_BUF_SIGNATURE, NULL, 0, 0, false, false }

#define TEN_BUF_STATIC_INIT_WITH_DATA_OWNED(data, size) \
  (ten_buf_t) { TEN_BUF_SIGNATURE, data, size, size, true, false }

#define TEN_BUF_STATIC_INIT_WITH_DATA_UNOWNED(data, size) \
  (ten_buf_t) { TEN_BUF_SIGNATURE, data, size, size, false, false }

// The concept of buf_t is simple: if a memory buffer is passed in during
// construction, then buf_t will not own that memory buffer because buf_t does
// not know how to release it. Using any memory release API in buf_t's
// destructor could potentially mismatch with the API the user originally used
// to create that memory buffer. Therefore, if buf_t's constructor parameters
// are void* and size_t, the behavior is not owning that memory buffer, as the
// destructor would not know how to free/delete/release that buffer. Conversely,
// if buf_t's constructor parameter is just a size, it implies that buf_t will
// internally create the memory buffer, and in this case, buf_t will have
// ownership of that memory buffer because buf_t knows how to release it. On the
// other hand, if the user were to release the buffer themselves, it would
// result in a similar mismatch between creation and release APIs.
typedef struct ten_buf_t {
  ten_signature_t signature;

  uint8_t *data;  //< The pointer points to the beginning of the buffer.
  size_t size;    //< The size of the allocated buffer.

  size_t content_size;  //< The size of the actual data contained in the buffer.
  bool owns_memory;     //< Whether if 'ten_buf_t' owns the containing buffer.
  bool is_fixed_size;   //< Whether if the size of the buffer is fixed.
} ten_buf_t;

TEN_UTILS_API bool ten_buf_check_integrity(ten_buf_t *self);

TEN_UTILS_API void ten_buf_reset(ten_buf_t *self);

/**
 * @brief Init a buffer.
 * @param size The size of buffer.
 */
TEN_UTILS_API bool ten_buf_init_with_owned_data(ten_buf_t *self, size_t size);

/**
 * @brief Initialize a ten_buf_t with @a size bytes of data.
 *
 * @param data The pointer of buffer.
 * @param size The size of buffer.
 */
TEN_UTILS_API bool ten_buf_init_with_unowned_data(ten_buf_t *self,
                                                  uint8_t *data, size_t size);

TEN_UTILS_API bool ten_buf_init_with_copying_data(ten_buf_t *self,
                                                  uint8_t *data, size_t size);

/**
 * @brief Create a ten_buf_t with @a size bytes.
 */
TEN_UTILS_API ten_buf_t *ten_buf_create_with_owned_data(size_t size);

/**
 * @brief De-init a ten_buf_t struct
 *
 * @param self The buf.
 */
TEN_UTILS_API void ten_buf_deinit(ten_buf_t *self);

/**
 * @brief Destroy a ten_buf_t struct
 *
 * @param self The buf.
 */
TEN_UTILS_API void ten_buf_destroy(ten_buf_t *self);

TEN_UTILS_API void ten_buf_move(ten_buf_t *self, ten_buf_t *other);

TEN_UTILS_API uint8_t *ten_buf_get_data(ten_buf_t *self);

TEN_UTILS_API size_t ten_buf_get_size(ten_buf_t *self);
