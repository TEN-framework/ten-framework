//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/value/value.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "include_internal/ten_utils/value/value.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/type.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_kv.h"

bool ten_value_check_integrity(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_VALUE_SIGNATURE) {
    return false;
  }
  return true;
}

static bool ten_value_copy_int8(ten_value_t *dest, ten_value_t *src,
                                TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_INT8, "Invalid argument.");

  dest->content.int8 = src->content.int8;

  return true;
}

static void ten_value_init(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  // Initialize all memory within `value` to 0, so that the type within it (such
  // as `ten_string_t`) recognizes it as being in an uninitialized state.
  memset(self, 0, sizeof(ten_value_t));

  ten_signature_set(&self->signature, (ten_signature_t)TEN_VALUE_SIGNATURE);
  self->type = TEN_TYPE_INVALID;
}

bool ten_value_init_int8(ten_value_t *self, int8_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_INT8;
  self->content.int8 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_int8;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_int16(ten_value_t *dest, ten_value_t *src,
                                 TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_INT16, "Invalid argument.");

  dest->content.int16 = src->content.int16;

  return true;
}

bool ten_value_init_int16(ten_value_t *self, int16_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_INT16;
  self->content.int16 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_int16;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_int32(ten_value_t *dest, ten_value_t *src,
                                 TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_INT32, "Invalid argument.");

  dest->content.int32 = src->content.int32;

  return true;
}

bool ten_value_init_int32(ten_value_t *self, int32_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_INT32;
  self->content.int32 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_int32;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_int64(ten_value_t *dest, ten_value_t *src,
                                 TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_INT64, "Invalid argument.");

  dest->content.int64 = src->content.int64;

  return true;
}

bool ten_value_init_int64(ten_value_t *self, int64_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_INT64;
  self->content.int64 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_int64;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_uint8(ten_value_t *dest, ten_value_t *src,
                                 TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_UINT8, "Invalid argument.");

  dest->content.uint8 = src->content.uint8;

  return true;
}

bool ten_value_init_uint8(ten_value_t *self, uint8_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_UINT8;
  self->content.uint8 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_uint8;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_uint16(ten_value_t *dest, ten_value_t *src,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_UINT16, "Invalid argument.");

  dest->content.uint16 = src->content.uint16;

  return true;
}

bool ten_value_init_uint16(ten_value_t *self, uint16_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_UINT16;
  self->content.uint16 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_uint16;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_uint32(ten_value_t *dest, ten_value_t *src,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_UINT32, "Invalid argument.");

  dest->content.uint32 = src->content.uint32;

  return true;
}

bool ten_value_init_uint32(ten_value_t *self, uint32_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_UINT32;
  self->content.uint32 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_uint32;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_uint64(ten_value_t *dest, ten_value_t *src,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_UINT64, "Invalid argument.");

  dest->content.uint64 = src->content.uint64;

  return true;
}

bool ten_value_init_uint64(ten_value_t *self, uint64_t value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_UINT64;
  self->content.uint64 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_uint64;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_float32(ten_value_t *dest, ten_value_t *src,
                                   TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_FLOAT32, "Invalid argument.");

  dest->content.float32 = src->content.float32;

  return true;
}

bool ten_value_init_float32(ten_value_t *self, float value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_FLOAT32;
  self->content.float32 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_float32;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_float64(ten_value_t *dest, ten_value_t *src,
                                   TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_FLOAT64, "Invalid argument.");

  dest->content.float64 = src->content.float64;

  return true;
}

bool ten_value_init_float64(ten_value_t *self, double value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_FLOAT64;
  self->content.float64 = value;

  self->construct = NULL;
  self->copy = ten_value_copy_float64;
  self->destruct = NULL;

  return true;
}

bool ten_value_init_null(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_NULL;

  return true;
}

static bool ten_value_copy_bool(ten_value_t *dest, ten_value_t *src,
                                TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_BOOL, "Invalid argument.");

  dest->content.boolean = src->content.boolean;

  return true;
}

bool ten_value_init_bool(ten_value_t *self, bool value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_BOOL;
  self->content.boolean = value;

  self->construct = NULL;
  self->copy = ten_value_copy_bool;
  self->destruct = NULL;

  return true;
}

static bool ten_value_copy_construct_string(ten_value_t *dest, ten_value_t *src,
                                            TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(ten_value_is_string(src), "Invalid argument.");

  ten_string_init_from_string(&dest->content.string, &src->content.string);

  return true;
}

static bool ten_value_destruct_string(ten_value_t *self,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_is_string(self), "Invalid argument.");

  ten_string_deinit(&self->content.string);

  return true;
}

bool ten_value_init_string(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_STRING;

  TEN_STRING_INIT(self->content.string);
  self->construct = NULL;
  self->copy = ten_value_copy_construct_string;
  self->destruct = ten_value_destruct_string;

  return true;
}

static bool ten_value_init_vastring(ten_value_t *self, const char *fmt,
                                    va_list ap) {
  TEN_ASSERT(fmt, "Invalid argument.");

  ten_value_init_string(self);

  ten_string_append_from_va_list(&self->content.string, fmt, ap);

  return true;
}

bool ten_value_init_string_with_size(ten_value_t *self, const char *str,
                                     size_t len) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(str, "Invalid argument.");

  ten_value_init_string(self);
  ten_string_set_formatted(&self->content.string, "%.*s", len, str);

  return true;
}

static bool ten_value_copy_array(ten_value_t *dest, ten_value_t *src,
                                 TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_ARRAY, "Invalid argument.");

  ten_list_init(&dest->content.array);
  ten_list_foreach (&src->content.array, iter) {
    ten_value_t *item = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(item && ten_value_check_integrity(item), "Invalid argument.");

    ten_value_t *clone_item = ten_value_clone(item);
    ten_list_push_ptr_back(&dest->content.array, clone_item,
                           (ten_ptr_listnode_destroy_func_t)ten_value_destroy);
  }

  return true;
}

static bool ten_value_destruct_array(ten_value_t *self,
                                     TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_ARRAY, "Invalid argument.");

  ten_list_clear(&self->content.array);

  return true;
}

bool ten_value_init_array_with_move(ten_value_t *self, ten_list_t *value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_ARRAY;
  ten_list_init(&self->content.array);
  if (value) {
    ten_list_swap(&self->content.array, value);
  }

  self->construct = NULL;
  self->copy = ten_value_copy_array;
  self->destruct = ten_value_destruct_array;

  return true;
}

static bool ten_value_copy_ptr_default(ten_value_t *dest, ten_value_t *src,
                                       TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_PTR, "Invalid argument.");

  dest->content.ptr = src->content.ptr;

  return true;
}

static void ten_value_init_ptr(ten_value_t *self, void *ptr,
                               ten_value_construct_func_t construct,
                               ten_value_copy_func_t copy,
                               ten_value_destruct_func_t destruct) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_PTR;
  self->content.ptr = ptr;

  self->construct = construct;
  // If there is a customized copy handler, use it, otherwise, simply copy the
  // pointer directly.
  self->copy = copy ? copy : ten_value_copy_ptr_default;
  self->destruct = destruct;

  // If there is a customized construct handler, use it, otherwise, just set the
  // pointer value.
  if (self->construct) {
    self->construct(self, NULL);
  }
}

static bool ten_value_copy_buf(ten_value_t *dest, ten_value_t *src,
                               TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_BUF, "Invalid argument.");

  ten_buf_t *src_buf = &src->content.buf;
  if (src_buf->owns_memory) {
    if (src_buf->size) {
      ten_buf_init_with_copying_data(&dest->content.buf, src_buf->data,
                                     src_buf->size);
    }
  } else {
    dest->content.buf = *src_buf;
  }

  return true;
}

static bool ten_value_destruct_buf(ten_value_t *self,
                                   TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_BUF, "Invalid argument.");

  ten_buf_t *buf = &self->content.buf;
  if (buf->owns_memory) {
    ten_buf_deinit(&self->content.buf);
  }

  return true;
}

bool ten_value_init_buf(ten_value_t *self, size_t size) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_BUF;
  ten_buf_init_with_owned_data(&self->content.buf, size);

  self->construct = NULL;
  self->copy = ten_value_copy_buf;
  self->destruct = ten_value_destruct_buf;

  return true;
}

static void ten_value_init_buf_with_move(ten_value_t *self, ten_buf_t buf) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_BUF;
  self->content.buf = buf;

  self->construct = NULL;
  self->copy = ten_value_copy_buf;
  self->destruct = ten_value_destruct_buf;
}

static bool ten_value_copy_object(ten_value_t *dest, ten_value_t *src,
                                  TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(dest && src, "Invalid argument.");
  TEN_ASSERT(src->type == TEN_TYPE_OBJECT, "Invalid argument.");

  ten_list_init(&dest->content.object);

  ten_list_foreach (&src->content.object, iter) {
    ten_value_kv_t *item = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(item && ten_value_kv_check_integrity(item), "Invalid argument.");

    ten_value_kv_t *clone_item = ten_value_kv_clone(item);
    ten_list_push_ptr_back(
        &dest->content.object, clone_item,
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  return true;
}

static bool ten_value_destruct_object(ten_value_t *self,
                                      TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(self->type == TEN_TYPE_OBJECT, "Invalid argument.");

  ten_list_clear(&self->content.object);

  return true;
}

bool ten_value_init_object_with_move(ten_value_t *self, ten_list_t *value) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  self->type = TEN_TYPE_OBJECT;
  ten_list_init(&self->content.object);
  if (value) {
    ten_list_swap(&self->content.object, value);
  }

  self->construct = NULL;
  self->copy = ten_value_copy_object;
  self->destruct = ten_value_destruct_object;

  return true;
}

void ten_value_reset_to_string_with_size(ten_value_t *self, const char *str,
                                         size_t len) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  ten_value_deinit(self);
  ten_value_init_string_with_size(self, str, len);
}

void ten_value_reset_to_null(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  ten_value_deinit(self);
  ten_value_init_null(self);
}

void ten_value_reset_to_ptr(ten_value_t *self, void *ptr,
                            ten_value_construct_func_t construct,
                            ten_value_copy_func_t copy,
                            ten_value_destruct_func_t destruct) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  ten_value_deinit(self);
  ten_value_init_ptr(self, ptr, construct, copy, destruct);
}

static ten_value_t *ten_value_create(void) {
  ten_value_t *self = (ten_value_t *)TEN_MALLOC(sizeof(ten_value_t));
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);

  return self;
}

ten_value_t *ten_value_create_invalid(void) {
  ten_value_t *v = ten_value_create();
  ten_value_init_invalid(v);
  return v;
}

bool ten_value_init_invalid(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_value_init(self);
  self->type = TEN_TYPE_INVALID;

  return true;
}

ten_value_t *ten_value_create_int8(int8_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_int8(self, value);
  return self;
}

ten_value_t *ten_value_create_int16(int16_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_int16(self, value);
  return self;
}

ten_value_t *ten_value_create_int32(int32_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_int32(self, value);
  return self;
}

ten_value_t *ten_value_create_int64(int64_t value) {
  ten_value_t *self = ten_value_create();
  bool rc = ten_value_init_int64(self, value);
  TEN_ASSERT(rc, "Failed to initialize the value.");
  return self;
}

ten_value_t *ten_value_create_uint8(uint8_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_uint8(self, value);
  return self;
}

ten_value_t *ten_value_create_uint16(uint16_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_uint16(self, value);
  return self;
}

ten_value_t *ten_value_create_uint32(uint32_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_uint32(self, value);
  return self;
}

ten_value_t *ten_value_create_uint64(uint64_t value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_uint64(self, value);
  return self;
}

ten_value_t *ten_value_create_float32(float value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_float32(self, value);
  return self;
}

ten_value_t *ten_value_create_float64(double value) {
  ten_value_t *self = ten_value_create();
  bool rc = ten_value_init_float64(self, value);
  TEN_ASSERT(rc, "Failed to initialize the value.");
  return self;
}

ten_value_t *ten_value_create_bool(bool value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_bool(self, value);
  return self;
}

ten_value_t *ten_value_create_null(void) {
  ten_value_t *self = ten_value_create();
  bool rc = ten_value_init_null(self);
  TEN_ASSERT(rc, "Failed to initialize the value.");
  return self;
}

ten_value_t *ten_value_create_array_with_move(ten_list_t *value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_array_with_move(self, value);
  return self;
}

ten_value_t *ten_value_create_object_with_move(ten_list_t *value) {
  ten_value_t *self = ten_value_create();
  ten_value_init_object_with_move(self, value);
  return self;
}

ten_value_t *ten_value_create_string_with_size(const char *str, size_t len) {
  TEN_ASSERT(str, "Invalid argument.");

  ten_value_t *self = ten_value_create();
  bool rc = ten_value_init_string_with_size(self, str, len);
  TEN_ASSERT(rc, "Failed to initialize the value.");
  return self;
}

ten_value_t *ten_value_create_string(const char *str) {
  TEN_ASSERT(str, "Invalid argument.");
  return ten_value_create_string_with_size(str, strlen(str));
}

ten_value_t *ten_value_create_vastring(const char *fmt, va_list ap) {
  TEN_ASSERT(fmt, "Invalid argument.");

  ten_value_t *self = ten_value_create();
  bool rc = ten_value_init_vastring(self, fmt, ap);
  TEN_ASSERT(rc, "Failed to initialize the value.");
  return self;
}

ten_value_t *ten_value_create_vstring(const char *fmt, ...) {
  TEN_ASSERT(fmt, "Invalid argument.");

  va_list ap;
  va_start(ap, fmt);
  ten_value_t *self = ten_value_create_vastring(fmt, ap);
  va_end(ap);

  return self;
}

void ten_value_set_name(ten_value_t *self, const char *fmt, ...) {
  TEN_ASSERT(self && fmt, "Invalid argument.");

  if (self->name) {
    ten_string_destroy(self->name);
  }

  va_list ap;
  va_start(ap, fmt);
  self->name = ten_string_create_from_va_list(fmt, ap);
  va_end(ap);
}

ten_value_t *ten_value_create_ptr(void *ptr,
                                  ten_value_construct_func_t construct,
                                  ten_value_copy_func_t copy,
                                  ten_value_destruct_func_t destruct) {
  ten_value_t *self = ten_value_create();
  ten_value_init_ptr(self, ptr, construct, copy, destruct);
  return self;
}

ten_value_t *ten_value_create_buf_with_move(ten_buf_t buf) {
  ten_value_t *self = ten_value_create();
  ten_value_init_buf_with_move(self, buf);
  return self;
}

bool ten_value_copy(ten_value_t *src, ten_value_t *dest) {
  TEN_ASSERT(src && ten_value_check_integrity(src), "Invalid argument.");
  TEN_ASSERT(dest && ten_value_check_integrity(dest), "Invalid argument.");

  dest->type = src->type;

  ten_value_construct_func_t construct = src->construct;
  ten_value_destruct_func_t destruct = src->destruct;
  ten_value_copy_func_t copy = src->copy;

  if (src->copy) {
    src->copy(dest, src, NULL);
  }

  dest->construct = construct;
  dest->destruct = destruct;
  dest->copy = copy;

  return true;
}

ten_value_t *ten_value_clone(ten_value_t *src) {
  TEN_ASSERT(src && ten_value_check_integrity(src), "Invalid argument.");

  ten_value_t *self = ten_value_create();

  bool rc = ten_value_copy(src, self);
  TEN_ASSERT(rc, "Should not happen.");

  return self;
}

void ten_value_deinit(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  if (self->name) {
    ten_string_destroy(self->name);
    self->name = NULL;
  }

  if (self->destruct) {
    self->destruct(self, NULL);
  }
}

void ten_value_destroy(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  ten_value_deinit(self);
  TEN_FREE(self);
}

size_t ten_value_array_size(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  return ten_list_size(&self->content.array);
}

bool ten_value_is_valid(ten_value_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");

  switch (self->type) {
  case TEN_TYPE_INVALID:
    return false;
  default:
    return true;
  }
}

bool ten_value_is_equal(ten_value_t *self, ten_value_t *target) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_value_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(target && ten_value_check_integrity(target), "Invalid argument.");

  if (self->type != target->type) {
    return false;
  }
  switch (self->type) {
  case TEN_TYPE_NULL:
    break;
  case TEN_TYPE_BOOL:
    return self->content.boolean == target->content.boolean;
  case TEN_TYPE_STRING:
    return ten_string_is_equal(&self->content.string, &target->content.string);
  case TEN_TYPE_OBJECT:
    if (ten_list_size(&self->content.object) !=
        ten_list_size(&target->content.object)) {
      return false;
    }

    ten_list_foreach (&self->content.object, iter_self) {
      ten_value_kv_t *kv_self = ten_ptr_listnode_get(iter_self.node);
      TEN_ASSERT(kv_self && ten_value_kv_check_integrity(kv_self),
                 "Invalid argument.");

      // Peek the corresponding value in the target object.
      ten_value_t *kv_target =
          ten_value_object_peek(target, ten_string_get_raw_str(&kv_self->key));
      if (!kv_target) {
        // Key does not exist in target
        return false;
      }

      // Recursively check equality of the values.
      if (!ten_value_is_equal(kv_self->value, kv_target)) {
        return false;
      }
    }
    break;
  case TEN_TYPE_ARRAY:
    if (ten_list_size(&self->content.array) !=
        ten_list_size(&target->content.array)) {
      return false;
    }

    {
      ten_list_iterator_t iter_self = ten_list_begin(&self->content.array);
      ten_list_iterator_t iter_target = ten_list_begin(&target->content.array);

      while (!ten_list_iterator_is_end(iter_self) &&
             !ten_list_iterator_is_end(iter_target)) {
        ten_value_t *value_self =
            (ten_value_t *)ten_list_iterator_to_listnode(iter_self);
        ten_value_t *value_target =
            (ten_value_t *)ten_list_iterator_to_listnode(iter_target);

        if (!ten_value_is_equal(value_self, value_target)) {
          return false;
        }

        iter_self = ten_list_iterator_next(iter_self);
        iter_target = ten_list_iterator_next(iter_target);
      }
    }
    break;
  case TEN_TYPE_INT8:
    return self->content.int8 == target->content.int8;
  case TEN_TYPE_INT16:
    return self->content.int16 == target->content.int16;
  case TEN_TYPE_INT32:
    return self->content.int32 == target->content.int32;
  case TEN_TYPE_INT64:
    return self->content.int64 == target->content.int64;
  case TEN_TYPE_UINT8:
    return self->content.uint8 == target->content.uint8;
  case TEN_TYPE_UINT16:
    return self->content.uint16 == target->content.uint16;
  case TEN_TYPE_UINT32:
    return self->content.uint32 == target->content.uint32;
  case TEN_TYPE_UINT64:
    return self->content.uint64 == target->content.uint64;
  case TEN_TYPE_FLOAT32:
    return self->content.float32 == target->content.float32;
  case TEN_TYPE_FLOAT64:
    return self->content.float64 == target->content.float64;
  default:
    TEN_ASSERT(0, "Invalid argument.");
    return false;
  }

  return true;
}
