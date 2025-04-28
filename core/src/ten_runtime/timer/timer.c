//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/timer/timer.h"

#include <stdint.h>
#include <stdlib.h>

#include "include_internal/ten_runtime/common/loc.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd/timer/cmd.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/timer/timer.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

static bool ten_timer_is_closing(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  return ten_atomic_load(&self->is_closing) == 1;
}

static bool ten_timer_could_be_close(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  if (self->backend) {
    return false;
  }
  return true;
}

static void ten_timer_do_close(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  if (self->on_closed) {
    self->on_closed(self, self->on_closed_data);
  }

  // All the necessary steps in the timer closing flow are done, so it's
  // safe to destroy the timer now to prevent memory leaks.
  ten_timer_destroy(self);
}

static void ten_timer_on_close(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  if (!ten_timer_could_be_close(self)) {
    TEN_LOGD("[%d] Could not close alive timer.", self->id);
    return;
  }

  TEN_LOGD("[%d] Timer can be closed now.", self->id);

  ten_timer_do_close(self);
}

static void ten_runloop_timer_on_closed(TEN_UNUSED ten_runloop_timer_t *timer,
                                        void *arg) {
  ten_timer_t *self = arg;

  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  // The runloop timer is closed, so it's safe to destroy the runloop timer now
  // to prevent memory leaks.
  ten_runloop_timer_destroy(self->backend);
  self->backend = NULL;

  if (ten_timer_is_closing(self)) {
    ten_timer_on_close(self);
  }
}

static void ten_timer_close(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  ten_runloop_timer_t *timer = self->backend;
  TEN_ASSERT(timer, "Should not happen.");

  ten_runloop_timer_close(timer, ten_runloop_timer_on_closed, self);
}

static void ten_runloop_timer_on_stopped(TEN_UNUSED ten_runloop_timer_t *timer,
                                         TEN_UNUSED void *arg) {}

static void ten_timer_stop(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  ten_runloop_timer_t *timer = self->backend;
  TEN_ASSERT(timer, "Should not happen.");

  ten_runloop_timer_stop(timer, ten_runloop_timer_on_stopped, self);
}

bool ten_timer_check_integrity(ten_timer_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_TIMER_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

static void ten_timer_on_trigger(ten_timer_t *self,
                                 TEN_UNUSED void *on_trigger_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  self->times++;

  if ((self->requested_times == TEN_TIMER_INFINITE) ||
      (self->times <= self->requested_times)) {
    if (self->on_trigger) {
      self->on_trigger(self, self->on_trigger_data);
    }

    if (!self->auto_restart) {
      // If the timer is _not_ auto_restart, it will not automatically start the
      // next round of timing or close itself after each timeout trigger.
      // Instead, it will only do so when the user manually enables the timer
      // again.
      return;
    }

    if (self->requested_times == TEN_TIMER_INFINITE ||
        self->times < self->requested_times) {
      // Setup the next timeout.
      ten_timer_enable(self);
    } else {
      ten_timer_stop_async(self);
      ten_timer_close_async(self);
    }
  } else {
    TEN_ASSERT(0, "Should not happen.");
  }
}

static ten_timer_t *ten_timer_create_internal(ten_runloop_t *runloop) {
  TEN_ASSERT(runloop && ten_runloop_check_integrity(runloop, true),
             "Should not happen.");

  TEN_LOGV("Create a timer.");

  ten_timer_t *self = (ten_timer_t *)TEN_MALLOC(sizeof(ten_timer_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_TIMER_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->runloop = runloop;

  self->id = 0;
  self->times = 0;
  self->auto_restart = true;

  ten_loc_init_empty(&self->src_loc);

  self->on_trigger = NULL;
  self->on_trigger_data = NULL;

  ten_atomic_store(&self->is_closing, 0);
  self->on_closed = NULL;
  self->on_closed_data = NULL;

  self->backend = ten_runloop_timer_create(NULL, 0, 0);
  if (!self->backend) {
    TEN_LOGE("No valid backend for timer.");
    goto error;
  }

  return self;

error:
  if (self) {
    if (self->backend) {
      ten_runloop_timer_destroy(self->backend);
    }

    TEN_FREE(self);
  }
  return NULL;
}

ten_timer_t *ten_timer_create(ten_runloop_t *runloop, uint64_t timeout_us,
                              int32_t requested_times, bool auto_restart) {
  TEN_ASSERT(runloop && ten_runloop_check_integrity(runloop, true),
             "Should not happen.");

  ten_timer_t *self = ten_timer_create_internal(runloop);
  if (!self) {
    return NULL;
  }

  self->timeout_us = timeout_us;
  self->requested_times = requested_times;
  self->auto_restart = auto_restart;

  return self;
}

ten_timer_t *ten_timer_create_with_cmd(ten_shared_ptr_t *cmd,
                                       ten_runloop_t *runloop) {
  TEN_ASSERT(cmd && ten_msg_get_type(cmd) == TEN_MSG_TYPE_CMD_TIMER &&
                 runloop && ten_runloop_check_integrity(runloop, true),
             "Should not happen.");

  ten_timer_t *self = ten_timer_create_internal(runloop);
  if (!self) {
    return NULL;
  }

  self->id = ten_cmd_timer_get_timer_id(cmd);
  self->timeout_us = ten_cmd_timer_get_timeout_us(cmd);
  self->requested_times = ten_cmd_timer_get_times(cmd);
  ten_loc_set_from_loc(&self->src_loc, ten_msg_get_src_loc(cmd));

  return self;
}

void ten_timer_destroy(ten_timer_t *self) {
  TEN_ASSERT(self && ten_timer_check_integrity(self, true) &&
                 ten_runloop_check_integrity(self->runloop, true) &&
                 ten_timer_could_be_close(self),
             "Should not happen.");

  TEN_LOGV("Destroy a timer.");

  ten_sanitizer_thread_check_deinit(&self->thread_check);
  ten_signature_set(&self->signature, 0);
  ten_loc_deinit(&self->src_loc);

  TEN_FREE(self);
}

static void ten_runloop_timer_on_triggered(
    TEN_UNUSED ten_runloop_timer_t *timer, void *arg) {
  ten_timer_t *self = arg;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  ten_timer_on_trigger(self, self->on_trigger_data);
}

void ten_timer_enable(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  if (self->requested_times != TEN_TIMER_INFINITE &&
      self->times >= self->requested_times) {
    // The timer has ended, so it should not be enabled again.
    ten_timer_stop_async(self);
    ten_timer_close_async(self);
    return;
  }

  ten_runloop_timer_set_timeout(self->backend, self->timeout_us / 1000, 0);

  ten_runloop_timer_start(self->backend, self->runloop,
                          ten_runloop_timer_on_triggered, self);
}

static void ten_timer_stop_task(void *timer_, TEN_UNUSED void *arg) {
  ten_timer_t *timer = (ten_timer_t *)timer_;
  TEN_ASSERT(timer, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(timer, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(timer->runloop, true),
             "Should not happen.");

  ten_timer_stop(timer);
}

/**
 * @brief Asynchronously stops a timer by posting a stop task to the timer's
 * runloop.
 *
 * This function schedules the timer to be stopped in the context of its own
 * runloop, making it thread-safe to call from any thread. The actual stopping
 * operation will be performed by the `ten_timer_stop_task` function when the
 * posted task is executed.
 *
 * Note: This function does not wait for the timer to actually stop. It only
 * schedules the stop operation.
 *
 * @param self The timer to stop.
 */
void ten_timer_stop_async(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");

  TEN_LOGV("Scheduling timer stop operation.");

  // Post the stop task to the timer's runloop to ensure thread safety
  int rc = ten_runloop_post_task_tail(self->runloop, ten_timer_stop_task, self,
                                      NULL);
  if (rc) {
    TEN_LOGW("Failed to post timer stop task to runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

void ten_timer_set_on_closed(ten_timer_t *self,
                             ten_timer_on_closed_func_t on_closed,
                             void *on_closed_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  self->on_closed = on_closed;
  self->on_closed_data = on_closed_data;
}

void ten_timer_set_on_triggered(ten_timer_t *self,
                                ten_timer_on_trigger_func_t on_trigger,
                                void *on_trigger_data) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  self->on_trigger = on_trigger;
  self->on_trigger_data = on_trigger_data;
}

static void ten_timer_close_task(void *timer_, TEN_UNUSED void *arg) {
  ten_timer_t *timer = (ten_timer_t *)timer_;
  TEN_ASSERT(timer, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(timer, true), "Should not happen.");

  ten_timer_close(timer);
}

void ten_timer_close_async(ten_timer_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_timer_check_integrity(self, true), "Should not happen.");

  if (ten_atomic_bool_compare_swap(&self->is_closing, 0, 1)) {
    TEN_LOGV("Try to close a timer.");

    int rc = ten_runloop_post_task_tail(self->runloop, ten_timer_close_task,
                                        self, NULL);
    if (rc) {
      TEN_LOGW("Failed to post task to timer's runloop: %d", rc);
      TEN_ASSERT(0, "Should not happen.");
    }
  }
}

bool ten_timer_is_id_equal_to(ten_timer_t *self, uint32_t id) {
  TEN_ASSERT(self && ten_timer_check_integrity(self, true) && id &&
                 ten_runloop_check_integrity(self->runloop, true),
             "Should not happen.");

  return (self->id == id) ? true : false;
}
