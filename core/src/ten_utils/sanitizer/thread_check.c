//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/sanitizer/thread_check.h"

#include <string.h>

#include "include_internal/ten_utils/sanitizer/thread_check.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

bool ten_sanitizer_thread_check_check_integrity(
    ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_SANITIZER_THREAD_CHECK_SIGNATURE) {
    return false;
  }
  return true;
}

void ten_sanitizer_thread_check_init(ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, TEN_SANITIZER_THREAD_CHECK_SIGNATURE);
  self->belonging_thread = NULL;
  self->is_fake = false;
}

void ten_sanitizer_thread_check_init_with_current_thread(
    ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_sanitizer_thread_check_init(self);

  self->belonging_thread = ten_thread_self();
  if (self->belonging_thread == NULL) {
    // The current thread was not created by ten_thread_create(). We create a
    // fake ten_thread_t with relevant information, but it doesn't actually
    // create a native thread.
    self->belonging_thread = ten_thread_create_fake("fake");
    self->is_fake = true;
  }
}

void ten_sanitizer_thread_check_init_from(ten_sanitizer_thread_check_t *self,
                                          ten_sanitizer_thread_check_t *other) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(other && ten_sanitizer_thread_check_check_integrity(other),
             "Invalid argument.");
  TEN_ASSERT(other->belonging_thread, "Should not happen.");

  ten_sanitizer_thread_check_init(self);
  ten_sanitizer_thread_check_inherit_from(self, other);
}

void ten_sanitizer_thread_check_set_belonging_thread(
    ten_sanitizer_thread_check_t *self, ten_thread_t *thread) {
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");

  if (self->belonging_thread && self->is_fake) {
    ten_thread_join_fake(self->belonging_thread);
    self->is_fake = false;
  }

  if (!thread) {
    thread = ten_thread_self();
  }

  self->belonging_thread = thread;
}

void ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
    ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");

  if (self->belonging_thread && self->is_fake) {
    ten_thread_join_fake(self->belonging_thread);
    self->is_fake = false;
  }

  self->belonging_thread = ten_thread_self();

  if (self->belonging_thread == NULL) {
    // The current thread was not created by ten_thread_create(). We create a
    // fake ten_thread_t with relevant information, but it doesn't actually
    // create a native thread.
    self->belonging_thread = ten_thread_create_fake("fake");
    self->is_fake = true;
  }
}

ten_thread_t *ten_sanitizer_thread_check_get_belonging_thread(
    ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");
  return self->belonging_thread;
}

void ten_sanitizer_thread_check_inherit_from(
    ten_sanitizer_thread_check_t *self, ten_sanitizer_thread_check_t *from) {
  TEN_ASSERT(from && ten_sanitizer_thread_check_check_integrity(from),
             "Should not happen.");
  TEN_ASSERT(from->belonging_thread, "Should not happen.");
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");

  if (self->belonging_thread && self->is_fake) {
    ten_thread_join_fake(self->belonging_thread);
    self->is_fake = false;
  }

  self->belonging_thread = from->belonging_thread;
}

bool ten_sanitizer_thread_check_do_check(ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");

  if (!self->belonging_thread) {
    // belonging_thread has not been set, which means we don't want to do thread
    // safety checks at this time.
    return true;
  }

  ten_thread_t *current_thread = ten_thread_self();

  if (ten_thread_equal(current_thread, self->belonging_thread)) {
    return true;
  }

  TEN_LOGE(
      "Access object across threads, current thread {id(%ld), name(%s)}, "
      "but belonging thread {id(%ld), name(%s)}.",
      (long)ten_thread_get_id(current_thread),
      ten_thread_get_name(current_thread),
      (long)ten_thread_get_id(self->belonging_thread),
      ten_thread_get_name(self->belonging_thread));

  return false;
}

void ten_sanitizer_thread_check_deinit(ten_sanitizer_thread_check_t *self) {
  TEN_ASSERT(self && ten_sanitizer_thread_check_check_integrity(self),
             "Should not happen.");

  if (self->is_fake) {
    ten_thread_join_fake(self->belonging_thread);
  }
  self->belonging_thread = NULL;
}
