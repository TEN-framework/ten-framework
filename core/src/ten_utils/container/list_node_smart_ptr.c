//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <stdlib.h>
#include <string.h>

#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

static bool ten_smart_ptr_listnode_check_integrity(
    ten_smart_ptr_listnode_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_SMART_PTR_LISTNODE_SIGNATURE) {
    return false;
  }
  return ten_listnode_check_integrity(&self->hdr);
}

static void ten_smart_ptr_listnode_destroy(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_smart_ptr_listnode_t *self = ten_listnode_to_smart_ptr_listnode(self_);
  ten_smart_ptr_destroy(self->ptr);
  self->ptr = NULL;
}

ten_listnode_t *ten_smart_ptr_listnode_create(ten_smart_ptr_t *ptr) {
  TEN_ASSERT(ptr, "Invalid argument.");

  ten_smart_ptr_listnode_t *self =
      (ten_smart_ptr_listnode_t *)ten_malloc(sizeof(ten_smart_ptr_listnode_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_listnode_init(&self->hdr, ten_smart_ptr_listnode_destroy);
  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_SMART_PTR_LISTNODE_SIGNATURE);
  self->ptr = ten_smart_ptr_clone(ptr);

  return (ten_listnode_t *)self;
}

ten_smart_ptr_listnode_t *ten_listnode_to_smart_ptr_listnode(
    ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_smart_ptr_listnode_t *self = (ten_smart_ptr_listnode_t *)self_;
  TEN_ASSERT(ten_smart_ptr_listnode_check_integrity(self), "Invalid argument.");

  return self;
}

ten_listnode_t *ten_listnode_from_smart_ptr_listnode(
    ten_smart_ptr_listnode_t *self) {
  TEN_ASSERT(self && ten_smart_ptr_listnode_check_integrity(self),
             "Invalid argument.");
  return &self->hdr;
}

ten_smart_ptr_t *ten_smart_ptr_listnode_get(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_smart_ptr_listnode_t *self = ten_listnode_to_smart_ptr_listnode(self_);
  return self->ptr;
}
