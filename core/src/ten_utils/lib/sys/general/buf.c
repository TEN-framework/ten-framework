//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/buf.h"

#include <stddef.h>
#include <string.h>

#include "include_internal/ten_utils/lib/buf.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

bool ten_buf_check_integrity(ten_buf_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_signature_get(&self->signature) != TEN_BUF_SIGNATURE) {
    return false;
  }

  return true;
}

static void ten_buf_init_empty(ten_buf_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature, TEN_BUF_SIGNATURE);

  self->data = NULL;
  self->content_size = 0;
  self->size = 0;
  self->owns_memory = true;
  self->is_fixed_size = false;
}

void ten_buf_deinit(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");

  if (self->owns_memory && self->data) {
    TEN_FREE(self->data);
  }

  ten_signature_set(&self->signature, 0);

  self->data = NULL;
  self->content_size = 0;
  self->size = 0;
  self->owns_memory = true;
  self->is_fixed_size = false;
}

void ten_buf_reset(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid arguments.");

  if (self->size > 0) {
    if (self->owns_memory) {
      memset(self->data, 0, self->size);
    } else {
      self->data = NULL;
      self->size = 0;
    }
  }

  self->content_size = 0;
  self->is_fixed_size = false;
}

bool ten_buf_init_with_owned_data(ten_buf_t *self, size_t size) {
  TEN_ASSERT(self, "Invalid argument.");
  if (!self) {
    return false;
  }

  ten_buf_init_empty(self);

  if (size != 0) {
    self->data = TEN_MALLOC(size);
    self->size = size;
  } else {
    self->data = NULL;
    self->size = 0;
  }

  self->owns_memory = true;

  return true;
}

bool ten_buf_init_with_unowned_data(ten_buf_t *self, uint8_t *data,
                                    size_t size) {
  TEN_ASSERT(self, "Invalid argument.");
  if (!self) {
    return false;
  }

  ten_buf_init_empty(self);

  self->data = data;
  self->content_size = size;
  self->size = size;

  self->owns_memory = false;

  return true;
}

bool ten_buf_init_with_copying_data(ten_buf_t *self, uint8_t *data,
                                    size_t size) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(data, "Invalid argument.");
  TEN_ASSERT(size, "Invalid argument.");
  if (!self) {
    return false;
  }

  ten_buf_init_empty(self);

  self->data = TEN_MALLOC(size);
  TEN_ASSERT(self->data, "Failed to allocate memory.");

  memcpy(self->data, data, size);
  self->content_size = size;
  self->size = size;

  self->owns_memory = true;

  return true;
}

ten_buf_t *ten_buf_create_with_owned_data(size_t size) {
  ten_buf_t *self = TEN_MALLOC(sizeof(ten_buf_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  if (!ten_buf_init_with_owned_data(self, size)) {
    TEN_FREE(self);
    return NULL;
  }

  return self;
}

void ten_buf_destroy(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");

  ten_buf_deinit(self);
  TEN_FREE(self);
}

void ten_buf_set_fixed_size(ten_buf_t *self, bool fixed) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->owns_memory,
             "Should not change the size of unowned buffer.");

  self->is_fixed_size = fixed;
}

bool ten_buf_reserve(ten_buf_t *self, size_t len) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->owns_memory,
             "Should not change the size of unowned buffer.");

  size_t req_size = self->content_size + len;
  if (req_size >= self->size) {
    if (self->is_fixed_size) {
      TEN_LOGE("Attempt to reserve more memory for a fixed-sized ten_buf_t.");
      TEN_ASSERT(0, "Should not happen.");
      return false;
    }

    if (self->size == 0) {
      self->size = req_size;
    }

    while (req_size >= self->size) {
      self->size *= 2;
    }

    self->data = TEN_REALLOC(self->data, self->size);
    TEN_ASSERT(self->data, "Failed to allocate memory.");
  }

  return true;
}

bool ten_buf_push(ten_buf_t *self, const uint8_t *src, size_t size) {
  TEN_ASSERT(self && ten_buf_check_integrity(self) && src, "Invalid argument.");
  TEN_ASSERT(self->owns_memory,
             "Should not change the size of unowned buffer.");

  if (!self) {
    return false;
  }

  if (!size) {
    return false;
  }

  if (!ten_buf_reserve(self, size)) {
    return false;
  }

  memcpy((char *)(self->data) + self->content_size, (char *)src, size);
  self->content_size += size;

  return true;
}

bool ten_buf_pop(ten_buf_t *self, uint8_t *dest, size_t size) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(self->owns_memory,
             "Should not change the size of unowned buffer.");

  if (!self) {
    return false;
  }

  if (!size) {
    return true;
  }

  if (size > self->content_size) {
    TEN_LOGW("Attempt to pop too many bytes from ten_buf_t.");
    return false;
  }

  size_t new_size = self->content_size - size;
  if (dest) {
    memcpy(dest, &self->data[new_size], size);
  }
  self->content_size = new_size;

  // Clear garbage in the discarded storage.
  memset(&self->data[new_size], 0, size);

  return true;
}

bool ten_buf_get_back(ten_buf_t *self, uint8_t *dest, size_t size) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  if (!self) {
    return false;
  }

  if (!size) {
    return true;
  }

  if (!dest) {
    TEN_LOGD("Attempt peek into a null pointer");
    return false;
  }

  if (size > self->content_size) {
    TEN_LOGW("Attempt to peek too many bytes from ten_buf_t.");
    return false;
  }

  memcpy(dest, &self->data[self->content_size - size], size);

  return true;
}

void ten_buf_take_ownership(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid arguments.");
  self->owns_memory = true;
}

void ten_buf_release_ownership(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid arguments.");
  self->owns_memory = false;
}

size_t ten_buf_get_content_size(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  return self->content_size;
}

size_t ten_buf_get_size(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  return self->size;
}

uint8_t *ten_buf_get_data(ten_buf_t *self) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  return self->data;
}

void ten_buf_move(ten_buf_t *self, ten_buf_t *other) {
  TEN_ASSERT(self && ten_buf_check_integrity(self), "Invalid argument.");
  TEN_ASSERT(other && ten_buf_check_integrity(self), "Invalid argument.");

  self->data = other->data;
  self->content_size = other->content_size;
  self->size = other->size;
  self->owns_memory = other->owns_memory;
  self->is_fixed_size = other->is_fixed_size;

  // The buf_t 'other' being moved to 'self' means that the ownership of the
  // memory has been transferred.
  other->owns_memory = false;

  ten_buf_init_with_owned_data(other, 0);
}
