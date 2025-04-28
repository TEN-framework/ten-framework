//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/backtrace/vector.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ten_vector_init(ten_vector_t *self, size_t capacity) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return;
  }

  self->size = 0;
  self->capacity = capacity > 0 ? capacity : 1;  // Ensure at least capacity 1.
  self->data = capacity > 0 ? malloc(self->capacity) : NULL;

  if (capacity > 0 && !self->data) {
    // Set capacity to 0 if allocation fails.
    self->capacity = 0;
  }
}

ten_vector_t *ten_vector_create(size_t capacity) {
  ten_vector_t *self = malloc(sizeof(ten_vector_t));
  if (!self) {
    assert(0 && "Failed to allocate memory.");
    return NULL;
  }

  ten_vector_init(self, capacity);
  // Check if initialization failed with non-zero capacity.
  if (capacity > 0 && !self->data) {
    free(self);
    return NULL;
  }

  return self;
}

/**
 * @brief Free the space managed by @a self. This will reset @a self.
 */
void ten_vector_deinit(ten_vector_t *self) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return;
  }

  // Free data if it exists
  if (self->data) {
    free(self->data);
    self->data = NULL;
  }

  self->size = 0;
  self->capacity = 0;
}

void ten_vector_destroy(ten_vector_t *self) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return;
  }

  ten_vector_deinit(self);

  free(self);
}

/**
 * @brief Check if size_t addition would overflow.
 */
static inline bool ten_size_add_would_overflow(size_t a, size_t b) {
  return b > SIZE_MAX - a;
}

/**
 * @brief Check if size_t multiplication would overflow.
 */
static inline bool ten_size_mul_would_overflow(size_t a, size_t b) {
  if (a == 0 || b == 0) {
    return false;
  }
  return a > SIZE_MAX / b;
}

/**
 * @brief Grow @a self by @a size bytes.
 *
 *                        grow space
 *                      v------v
 * -----------------------------
 * |                    |      |
 * -----------------------------
 *                      ^
 *                      returned address
 *
 * @return NULL if operation fails, pointer to the newly allocated memory
 * otherwise
 */
void *ten_vector_grow(ten_vector_t *self, size_t size) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return NULL;
  }

  // Check for zero size request.
  if (size == 0) {
    assert(0 && "Invalid argument.");
    return NULL;
  }

  // Check if adding size to self->size would overflow.
  if (ten_size_add_would_overflow(self->size, size)) {
    assert(0 && "Overflow.");
    return NULL;
  }

  size_t remaining =
      self->capacity > self->size ? self->capacity - self->size : 0;

  if (size > remaining) {
    size_t alc = 0;

    if (self->size == 0) {
      // Initially allocate 32 spaces with 'size' size.
      if (ten_size_mul_would_overflow(32, size)) {
        assert(0 && "Overflow.");
        alc = size;  // Fall back to exact size if would overflow.
      } else {
        alc = 32 * size;
      }
    } else if (self->size >= 4096) {
      // If the vector contains at least 4096 bytes, increase 4096 bytes each
      // time.
      if (ten_size_add_would_overflow(self->size, 4096)) {
        assert(0 && "Overflow.");
        return NULL;
      }
      alc = self->size + 4096;
    } else {
      // If the size of the vector is between 0 and 4096, it will be doubled
      // each time.
      if (ten_size_mul_would_overflow(2, self->size)) {
        assert(0 && "Overflow.");
        return NULL;
      }
      alc = 2 * self->size;
    }

    // Ensure we have enough space for the requested size.
    if (alc < self->size + size) {
      if (ten_size_add_would_overflow(self->size, size)) {
        assert(0 && "Overflow.");
        return NULL;
      }
      alc = self->size + size;
    }

    void *new_base = NULL;

    if (self->data == NULL) {
      new_base = malloc(alc);
    } else {
      new_base = realloc(self->data, alc);
    }

    if (!new_base) {
      assert(0 && "Failed to reallocate memory.");
      return NULL;
    }

    self->data = new_base;
    self->capacity = alc;
  }

  void *ret = (char *)self->data + self->size;
  self->size += size;

  return ret;
}

/**
 * @brief Release any remaining space allocated for @a self.
 * @return true on success, false on failure
 */
bool ten_vector_release_remaining_space(ten_vector_t *self) {
  if (!self) {
    assert(0 && "Invalid argument.");
    return false;
  }

  if (self->size == 0) {
    // Free and set to NULL if size is zero.
    if (self->data) {
      free(self->data);
      self->data = NULL;
    }
    self->capacity = 0;
    return true;
  }

  if (self->data == NULL) {
    assert(0 && "Should not happen.");
    return false;
  }

  if (self->size == self->capacity) {
    // Nothing to do if no extra space.
    return true;
  }

  void *new_data = realloc(self->data, self->size);
  if (!new_data) {
    assert(0 && "Failed to reallocate memory.");
    return false;
  }

  self->data = new_data;
  self->capacity = self->size;
  return true;
}

/**
 * @brief Shrink the data in @a self, and take it out of @a self.
 * @return NULL if operation fails, pointer to the data otherwise.
 */
void *ten_vector_take_out(ten_vector_t *self) {
  if (!self || !self->data) {
    assert(0 && "Invalid argument.");
    return NULL;
  }

  if (!ten_vector_release_remaining_space(self)) {
    return NULL;
  }

  void *ret = self->data;

  // Emulate the operation of taking the data out of the vector.
  self->data = NULL;
  self->size = 0;
  self->capacity = 0;

  return ret;
}
