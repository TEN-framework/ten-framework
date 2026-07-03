//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <float.h>
#include <stdint.h>
#include <stdlib.h>

#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/type_operation.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

int8_t ten_value_get_int8(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_INT8:
    return self->content.int8;
  case TEN_TYPE_INT16:
    if (self->content.int16 >= -INT8_MAX && self->content.int16 <= INT8_MAX) {
      return (int8_t)self->content.int16;
    }
    break;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= -INT8_MAX && self->content.int32 <= INT8_MAX) {
      return (int8_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= -INT8_MAX && self->content.int64 <= INT8_MAX) {
      return (int8_t)self->content.int64;
    }
    break;
  case TEN_TYPE_UINT8:
    if (self->content.uint8 <= INT8_MAX) {
      return (int8_t)self->content.uint8;
    }
    break;
  case TEN_TYPE_UINT16:
    if (self->content.uint16 <= INT8_MAX) {
      return (int8_t)self->content.uint16;
    }
    break;
  case TEN_TYPE_UINT32:
    if (self->content.uint32 <= INT8_MAX) {
      return (int8_t)self->content.uint32;
    }
    break;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= INT8_MAX) {
      return (int8_t)self->content.uint64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

int16_t ten_value_get_int16(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_INT8:
    return self->content.int8;
  case TEN_TYPE_INT16:
    return self->content.int16;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= -INT16_MAX && self->content.int32 <= INT16_MAX) {
      return (int16_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= -INT16_MAX && self->content.int64 <= INT16_MAX) {
      return (int16_t)self->content.int64;
    }
    break;
  case TEN_TYPE_UINT8:
    return (int16_t)self->content.uint8;
  case TEN_TYPE_UINT16:
    if (self->content.uint16 <= INT16_MAX) {
      return (int16_t)self->content.uint16;
    }
    break;
  case TEN_TYPE_UINT32:
    if (self->content.uint32 <= INT16_MAX) {
      return (int16_t)self->content.uint32;
    }
    break;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= INT16_MAX) {
      return (int16_t)self->content.uint64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

int32_t ten_value_get_int32(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_INT8:
    return self->content.int8;
  case TEN_TYPE_INT16:
    return self->content.int16;
  case TEN_TYPE_INT32:
    return self->content.int32;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= -INT32_MAX && self->content.int64 <= INT32_MAX) {
      return (int32_t)self->content.int64;
    }
    break;
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16;
  case TEN_TYPE_UINT32:
    if (self->content.uint32 <= INT32_MAX) {
      return (int32_t)self->content.uint32;
    }
    break;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= INT32_MAX) {
      return (int32_t)self->content.uint64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

int64_t ten_value_get_int64(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_INT8:
    return self->content.int8;
  case TEN_TYPE_INT16:
    return self->content.int16;
  case TEN_TYPE_INT32:
    return self->content.int32;
  case TEN_TYPE_INT64:
    return self->content.int64;
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16;
  case TEN_TYPE_UINT32:
    return self->content.uint32;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= INT64_MAX) {
      return (int64_t)self->content.uint64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

uint8_t ten_value_get_uint8(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    if (self->content.uint64 <= UINT8_MAX) {
      return (uint8_t)self->content.uint16;
    }
    break;
  case TEN_TYPE_UINT32:
    if (self->content.uint64 <= UINT8_MAX) {
      return (uint8_t)self->content.uint32;
    }
    break;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= UINT8_MAX) {
      return (uint8_t)self->content.uint64;
    }
    break;
  case TEN_TYPE_INT8:
    if (self->content.int8 >= 0) {
      return (uint8_t)self->content.int8;
    }
    break;
  case TEN_TYPE_INT16:
    if (self->content.int16 >= 0 && self->content.int16 <= UINT8_MAX) {
      return (uint8_t)self->content.int16;
    }
    break;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= 0 && self->content.int32 <= UINT8_MAX) {
      return (uint8_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= 0 && self->content.int64 <= UINT8_MAX) {
      return (uint8_t)self->content.int64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

uint16_t ten_value_get_uint16(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16;
  case TEN_TYPE_UINT32:
    if (self->content.uint64 <= UINT16_MAX) {
      return (uint16_t)self->content.uint32;
    }
    break;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= UINT16_MAX) {
      return (uint16_t)self->content.uint64;
    }
    break;
  case TEN_TYPE_INT8:
    if (self->content.int8 >= 0) {
      return (uint16_t)self->content.int8;
    }
    break;
  case TEN_TYPE_INT16:
    if (self->content.int16 >= 0) {
      return (uint16_t)self->content.int16;
    }
    break;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= 0 && self->content.int64 <= UINT16_MAX) {
      return (uint16_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= 0 && self->content.int64 <= UINT16_MAX) {
      return (uint16_t)self->content.int64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

uint32_t ten_value_get_uint32(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16;
  case TEN_TYPE_UINT32:
    return self->content.uint32;
  case TEN_TYPE_UINT64:
    if (self->content.uint64 <= UINT32_MAX) {
      return (uint32_t)self->content.uint64;
    }
    break;
  case TEN_TYPE_INT8:
    if (self->content.int8 >= 0) {
      return (uint32_t)self->content.int8;
    }
    break;
  case TEN_TYPE_INT16:
    if (self->content.int16 >= 0) {
      return (uint32_t)self->content.int16;
    }
    break;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= 0) {
      return (uint32_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= 0 && self->content.int64 <= UINT32_MAX) {
      return (uint32_t)self->content.int64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

uint64_t ten_value_get_uint64(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0;
  }

  switch (self->type) {
  case TEN_TYPE_UINT8:
    return self->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16;
  case TEN_TYPE_UINT32:
    return self->content.uint32;
  case TEN_TYPE_UINT64:
    return self->content.uint64;
  case TEN_TYPE_INT8:
    if (self->content.int8 >= 0) {
      return (uint64_t)self->content.int8;
    }
    break;
  case TEN_TYPE_INT16:
    if (self->content.int16 >= 0) {
      return (uint64_t)self->content.int16;
    }
    break;
  case TEN_TYPE_INT32:
    if (self->content.int32 >= 0) {
      return (uint64_t)self->content.int32;
    }
    break;
  case TEN_TYPE_INT64:
    if (self->content.int64 >= 0) {
      return (uint64_t)self->content.int64;
    }
    break;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0;
}

float ten_value_get_float32(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0.0F;
  }

  switch (self->type) {
  case TEN_TYPE_FLOAT32:
    return self->content.float32;
  case TEN_TYPE_FLOAT64:
    if (self->content.float64 >= -FLT_MAX && self->content.float64 <= FLT_MAX) {
      return (float)self->content.float64;
    }
    break;
  default:
    return 0.0F;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to float32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0.0F;
}

double ten_value_get_float64(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return 0.0F;
  }

  switch (self->type) {
  case TEN_TYPE_FLOAT32:
    return self->content.float32;
  case TEN_TYPE_FLOAT64:
    return self->content.float64;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to float32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return 0.0;
}

bool ten_value_get_bool(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_valid(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC, "Invalid value type.");
    }
    return false;
  }

  switch (self->type) {
  case TEN_TYPE_BOOL:
    return self->content.boolean;
  default:
    break;
  }

  if (err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "The conversion from %s to uint32 is unfit.",
                  ten_type_to_string(self->type));
  }
  return false;
}

void *ten_value_get_ptr(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_ptr(self)) {
    return self->content.ptr;
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Not pointer value, actual type: %s",
                    ten_type_to_string(self->type));
    }
    return NULL;
  }
}

ten_buf_t *ten_value_peek_buf(ten_value_t *self, ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_buf(self)) {
    return &self->content.buf;
  } else {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Not buffer value, actual type: %s",
                    ten_type_to_string(self->type));
    }
    return NULL;
  }
}

ten_list_t *ten_value_peek_array(ten_value_t *self) {
  if (!self) {
    return NULL;
  }

  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_array(self)) {
    return &self->content.array;
  }
  return NULL;
}

ten_list_t *ten_value_peek_object(ten_value_t *self) {
  if (!self) {
    return NULL;
  }

  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_object(self)) {
    return &self->content.object;
  }
  return NULL;
}

ten_string_t *ten_value_peek_string(ten_value_t *self) {
  if (!self) {
    return NULL;
  }

  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_string(self)) {
    return &self->content.string;
  } else {
    return NULL;
  }
}

const char *ten_value_peek_raw_str(ten_value_t *self, ten_error_t *err) {
  if (!self) {
    return NULL;
  }

  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (ten_value_is_string(self)) {
    return ten_string_get_raw_str(&self->content.string);
  }
  return NULL;
}

TEN_TYPE ten_value_get_type(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return self->type;
}

ten_value_t *ten_value_array_peek(ten_value_t *self, size_t index,
                                  ten_error_t *err) {
  if (!self) {
    return NULL;
  }

  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (!ten_value_is_array(self)) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "The conversion from %s to array is unfit.",
                    ten_type_to_string(self->type));
    }
    return NULL;
  }

  if (index >= ten_list_size(&self->content.array)) {
    return NULL;
  }

  ten_value_array_foreach(self, iter) {
    if (iter.index == index) {
      return ten_ptr_listnode_get(iter.node);
    }
  }

  TEN_ASSERT(0, "Should not happen.");
  return NULL;
}
