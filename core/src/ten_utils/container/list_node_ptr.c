//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/container/list_node.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

static bool ten_ptr_listnode_check_integrity(ten_ptr_listnode_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_NORMAL_PTR_LISTNODE_SIGNATURE) {
    return false;
  }
  return ten_listnode_check_integrity(&self->hdr);
}

static void ten_ptr_listnode_destroy(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_ptr_listnode_t *self = ten_listnode_to_ptr_listnode(self_);
  if (self->destroy) {
    self->destroy(self->ptr);
  }
}

ten_listnode_t *ten_ptr_listnode_create(
    void *ptr, ten_ptr_listnode_destroy_func_t destroy) {
  TEN_ASSERT(ptr, "Invalid argument.");

  ten_ptr_listnode_t *self =
      (ten_ptr_listnode_t *)ten_malloc(sizeof(ten_ptr_listnode_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_listnode_init(&self->hdr, ten_ptr_listnode_destroy);
  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_NORMAL_PTR_LISTNODE_SIGNATURE);
  self->ptr = ptr;
  self->destroy = destroy;

  return (ten_listnode_t *)self;
}

ten_listnode_t *ten_listnode_from_ptr_listnode(ten_ptr_listnode_t *self) {
  TEN_ASSERT(self && ten_ptr_listnode_check_integrity(self),
             "Invalid argument.");
  return &self->hdr;
}

ten_ptr_listnode_t *ten_listnode_to_ptr_listnode(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_ptr_listnode_t *self = (ten_ptr_listnode_t *)self_;
  TEN_ASSERT(ten_ptr_listnode_check_integrity(self), "Invalid argument.");
  return self;
}

void *ten_ptr_listnode_get(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_ptr_listnode_t *self = ten_listnode_to_ptr_listnode(self_);
  return self->ptr;
}

void ten_ptr_listnode_replace(ten_listnode_t *self_, void *ptr,
                              ten_ptr_listnode_destroy_func_t destroy) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_ptr_listnode_t *self = ten_listnode_to_ptr_listnode(self_);
  self->ptr = ptr;
  self->destroy = destroy;
}
