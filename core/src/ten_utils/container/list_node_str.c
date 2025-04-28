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
#include "ten_utils/macro/check.h"

static bool ten_str_listnode_check_integrity(ten_str_listnode_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_STR_LISTNODE_SIGNATURE) {
    return false;
  }
  return ten_listnode_check_integrity(&self->hdr);
}

static void ten_str_listnode_destroy(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_str_listnode_t *self = ten_listnode_to_str_listnode(self_);
  ten_string_deinit(&self->str);
}

ten_listnode_t *ten_str_listnode_create(const char *str) {
  TEN_ASSERT(str, "Invalid argument.");

  return ten_str_listnode_create_with_size(str, strlen(str));
}

ten_listnode_t *ten_str_listnode_create_with_size(const char *str,
                                                  size_t size) {
  TEN_ASSERT(str, "Invalid argument.");

  ten_str_listnode_t *self =
      (ten_str_listnode_t *)ten_malloc(sizeof(ten_str_listnode_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_listnode_init(&self->hdr, ten_str_listnode_destroy);
  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_STR_LISTNODE_SIGNATURE);

  ten_string_init_formatted(&self->str, "%.*s", size, str);

  return (ten_listnode_t *)self;
}

ten_listnode_t *ten_listnode_from_str_listnode(ten_str_listnode_t *self) {
  TEN_ASSERT(self && ten_str_listnode_check_integrity(self),
             "Invalid argument.");
  return &self->hdr;
}

ten_str_listnode_t *ten_listnode_to_str_listnode(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_str_listnode_t *self = (ten_str_listnode_t *)self_;
  TEN_ASSERT(ten_str_listnode_check_integrity(self), "Invalid argument.");
  return self;
}

ten_string_t *ten_str_listnode_get(ten_listnode_t *self_) {
  TEN_ASSERT(self_, "Invalid argument.");

  ten_str_listnode_t *self = ten_listnode_to_str_listnode(self_);
  return &self->str;
}
