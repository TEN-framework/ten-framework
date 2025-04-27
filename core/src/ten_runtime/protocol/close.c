//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/protocol/close.h"

#include "include_internal/ten_runtime/protocol/protocol.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

/**
 * @brief Describes the protocol chain relationship and closing process.
 *
 * The protocol chain has the following structure:
 *
 *   base protocol (ten_protocol_t) -->
 *                 implementation protocol (inherited from ten_protocol_t)
 *
 * A base protocol can only be closed when its implementation protocol has been
 * closed. The closing process occurs in two stages:
 *
 * - Stage 1: Top-down 'Need to close' notification
 *
 *   TEN world -> (notify close) ->
 *                base protocol -> (notify close) ->
 *                                 implementation protocol
 *
 * - Stage 2: Bottom-up 'I am closed' notification
 *
 *   implementation protocol -> (notify closed) ->
 *   base protocol -> (notify closed) ->
 *   TEN world
 *
 * A protocol (whether base or implementation) can only be fully closed when all
 * resources it holds have been properly released. This ensures orderly shutdown
 * and prevents resource leaks.
 */
static bool ten_protocol_could_be_close(ten_protocol_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");

  // Currently, the only underlying resource of a base protocol is its
  // implementation protocol. This function is called only after the
  // implementation protocol has been closed, so it always returns true.
  //
  // Future design considerations:
  // - If additional underlying resources are added to the base protocol, this
  //   function will be called whenever any resource is closed.
  // - Each resource should have a corresponding field in the base protocol to
  //   track its state.
  // - This function would then check all resource state fields to determine if
  //   everything has been properly closed before returning true.
  return true;
}

/**
 * @brief Completes the protocol closing process if all resources are released.
 *
 * This function is called when a resource of the protocol has been closed.
 * It checks if all resources have been properly released using
 * `ten_protocol_could_be_close()`. If resources are still active,
 * the close operation is deferred. Otherwise, it proceeds with closing the
 * protocol by changing its state to CLOSED and invoking the registered
 * `on_closed` callback.
 *
 * This function implements Stage 2 of the protocol closing process (bottom-up
 * notification that the protocol is fully closed).
 *
 * @param self The protocol to close.
 */
void ten_protocol_on_close(ten_protocol_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");

  if (!ten_protocol_could_be_close(self)) {
    TEN_LOGD("[%s] Could not close alive base protocol.",
             ten_string_get_raw_str(&self->uri));
    return;
  }

  TEN_LOGD("[%s] Base protocol can be closed now.",
           ten_string_get_raw_str(&self->uri));

  self->state = TEN_PROTOCOL_STATE_CLOSED;

  if (self->on_closed) {
    // Call the registered `on_closed` callback when the base protocol is
    // closed.
    self->on_closed(self, self->on_closed_user_data);
  }
}

void ten_protocol_set_on_closed(ten_protocol_t *self,
                                ten_protocol_on_closed_func_t on_closed,
                                void *on_closed_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");

  self->on_closed = on_closed;
  self->on_closed_user_data = on_closed_data;
}

void ten_protocol_close(ten_protocol_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");

  if (self->state >= TEN_PROTOCOL_STATE_CLOSING) {
    TEN_LOGD("[%s] Protocol is closing, do not close again.",
             ten_string_get_raw_str(&self->uri));
    return;
  }

  self->state = TEN_PROTOCOL_STATE_CLOSING;

  switch (self->role) {
  case TEN_PROTOCOL_ROLE_LISTEN:
    TEN_LOGD("[%s] Try to close listening protocol.",
             ten_string_get_raw_str(&self->uri));
    break;
  case TEN_PROTOCOL_ROLE_IN_INTERNAL:
  case TEN_PROTOCOL_ROLE_IN_EXTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_INTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_EXTERNAL:
    TEN_LOGD("[%s] Try to close communication protocol.",
             ten_string_get_raw_str(&self->uri));
    break;
  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  self->close(self);
}
