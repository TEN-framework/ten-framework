//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/protocol/close.h"

#include "include_internal/ten_runtime/protocol/integrated/close.h"
#include "include_internal/ten_runtime/protocol/integrated/protocol_integrated.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/timer/timer.h"
#include "ten_utils/io/stream.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

/**
 * @brief Determines if an integrated protocol can be safely closed.
 *
 * This function checks if all resources associated with the protocol have been
 * properly released. A protocol can only be closed when it no longer holds any
 * active resources. The specific resources checked depend on the protocol's
 * role:
 *
 * - For listening protocols: the listening transport must be released
 * - For communication protocols: both the communication stream and any retry
 *   timers must be released
 *
 * @param self The integrated protocol to check
 * @return true if the protocol can be closed, false if it still has active
 * resources
 */
static bool ten_protocol_integrated_could_be_close(
    ten_protocol_integrated_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_protocol_t *base_protocol = &self->base;
  TEN_ASSERT(base_protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(base_protocol, true),
             "Should not happen.");
  TEN_ASSERT(base_protocol->role != TEN_PROTOCOL_ROLE_INVALID,
             "Should not happen.");

  // Check resources according to different roles.
  switch (base_protocol->role) {
  case TEN_PROTOCOL_ROLE_LISTEN:
    if (self->role_facility.listening_transport) {
      return false;
    }
    break;

  case TEN_PROTOCOL_ROLE_IN_INTERNAL:
  case TEN_PROTOCOL_ROLE_IN_EXTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_INTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_EXTERNAL:
    if (self->role_facility.communication_stream) {
      return false;
    }

    if (self->retry_timer) {
      return false;
    }

    if (self->base.has_uncompleted_async_task) {
      return false;
    }
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return true;
}

/**
 * @brief Attempts to close an integrated protocol if all its resources have
 * been released.
 *
 * This function is called when the protocol is in the closing state. It checks
 * if all resources have been properly released using
 * `ten_protocol_integrated_could_be_close()`. If resources are still active,
 * the close operation is deferred. Otherwise, it proceeds with closing the
 * protocol by calling `ten_protocol_on_close()`.
 *
 * @param self The integrated protocol to close.
 */
void ten_protocol_integrated_on_close(ten_protocol_integrated_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_protocol_t *protocol = &self->base;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "Should not happen.");
  TEN_ASSERT(protocol->role != TEN_PROTOCOL_ROLE_INVALID, "Should not happen.");
  TEN_ASSERT(protocol->state == TEN_PROTOCOL_STATE_CLOSING,
             "Should not happen.");

  if (!ten_protocol_integrated_could_be_close(self)) {
    TEN_LOGD("[%s] Could not close alive integrated protocol.",
             ten_string_get_raw_str(&protocol->uri));
    return;
  }

  TEN_LOGD("[%s] Integrated protocol can be closed now.",
           ten_string_get_raw_str(&protocol->uri));

  ten_protocol_on_close(&self->base);
}

void ten_protocol_integrated_on_stream_closed(ten_protocol_integrated_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_protocol_t *protocol = &self->base;
  TEN_ASSERT(protocol, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true), "Invalid argument.");
  TEN_ASSERT(ten_protocol_role_is_communication(protocol),
             "Should not happen.");

  // Remember that this resource is closed.
  self->role_facility.communication_stream = NULL;

  if (protocol->state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_protocol_integrated_on_close(self);
  }
}

void ten_protocol_integrated_on_transport_closed(
    ten_protocol_integrated_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_protocol_t *protocol = &self->base;
  TEN_ASSERT(protocol, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(protocol, true), "Invalid argument.");
  TEN_ASSERT(protocol->role == TEN_PROTOCOL_ROLE_LISTEN, "Should not happen.");

  // Remember that this resource is closed.
  self->role_facility.listening_transport = NULL;

  if (protocol->state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_protocol_integrated_on_close(self);
  }
}

/**
 * @brief Protocol closing process flow.
 *
 * The protocol closing process happens in two stages:
 *
 * Stage 1: Initiating the close (top-down)
 *   - TEN runtime notifies the base protocol to close.
 *   - Base protocol notifies the integrated protocol to close.
 *   - Each layer begins shutting down its resources.
 *
 * Stage 2: Confirming closure (bottom-up)
 *   - Integrated protocol notifies base protocol when its resources are closed.
 *   - Base protocol notifies TEN runtime when it's fully closed.
 *   - TEN runtime can then proceed with any dependent cleanup.
 *
 * The function below (ten_protocol_integrated_close) implements Stage 1,
 * while the functions above (ten_protocol_integrated_on_close) implement the
 * callbacks for Stage 2.
 */
void ten_protocol_integrated_close(ten_protocol_integrated_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_protocol_t *base_protocol = &self->base;
  TEN_ASSERT(base_protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(base_protocol, true),
             "Should not happen.");
  TEN_ASSERT(base_protocol->state == TEN_PROTOCOL_STATE_CLOSING,
             "Should not happen.");

  bool has_pending_close_operations = false;

  switch (base_protocol->role) {
  case TEN_PROTOCOL_ROLE_LISTEN:
    if (self->role_facility.listening_transport) {
      ten_transport_close(self->role_facility.listening_transport);
      has_pending_close_operations = true;
    }
    break;

  case TEN_PROTOCOL_ROLE_IN_INTERNAL:
  case TEN_PROTOCOL_ROLE_IN_EXTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_INTERNAL:
  case TEN_PROTOCOL_ROLE_OUT_EXTERNAL:
    if (self->role_facility.communication_stream) {
      ten_stream_close(self->role_facility.communication_stream);
      has_pending_close_operations = true;
    }

    if (self->retry_timer) {
      ten_timer_stop_async(self->retry_timer);
      ten_timer_close_async(self->retry_timer);
      has_pending_close_operations = true;
    }
    break;

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  if (!has_pending_close_operations) {
    // If there is no any further closing operations submitted, it means the
    // integrated protocol could proceed its closing flow.
    ten_protocol_integrated_on_close(self);
  }
}
