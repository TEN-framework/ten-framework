//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

static bool ten_int32_listnode_check_integrity(ten_int32_listnode_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_INT32_LISTNODE_SIGNATURE) {
    return false;
  }
  return ten_listnode_check_integrity(&self->hdr);
}

static void ten_int32_listnode_destroy(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");
}

ten_listnode_t *ten_int32_listnode_create(int32_t int32) {
  ten_int32_listnode_t *self =
      (ten_int32_listnode_t *)ten_malloc(sizeof(ten_int32_listnode_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_listnode_init(&self->hdr, ten_int32_listnode_destroy);
  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_INT32_LISTNODE_SIGNATURE);
  self->int32 = int32;

  return (ten_listnode_t *)self;
}

ten_listnode_t *ten_listnode_from_int32_listnode(ten_int32_listnode_t *self) {
  TEN_ASSERT(self && ten_int32_listnode_check_integrity(self),
             "Invalid argument.");
  return &self->hdr;
}

ten_int32_listnode_t *ten_listnode_to_int32_listnode(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_int32_listnode_t *self = (ten_int32_listnode_t *)self_;
  TEN_ASSERT(ten_int32_listnode_check_integrity(self), "Invalid argument.");
  return self;
}

int32_t ten_int32_listnode_get(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_int32_listnode_t *self = ten_listnode_to_int32_listnode(self_);
  return self->int32;
}
