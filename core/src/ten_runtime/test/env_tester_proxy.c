//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/test/env_tester_proxy.h"

#include "include_internal/ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "include_internal/ten_runtime/test/env_tester.h"
#include "include_internal/ten_runtime/test/env_tester_proxy.h"
#include "include_internal/ten_runtime/test/extension_tester.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"

static bool ten_env_tester_proxy_check_integrity(ten_env_tester_proxy_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_ENV_TESTER_PROXY_SIGNATURE) {
    return false;
  }

  return true;
}

ten_env_tester_proxy_t *ten_env_tester_proxy_create(
    ten_env_tester_t *ten_env_tester, ten_error_t *err) {
  if (!ten_env_tester) {
    const char *err_msg =
        "Create ten_env_tester_proxy without specifying ten_env_tester.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return NULL;
  }

  ten_env_tester_proxy_t *self = TEN_MALLOC(sizeof(ten_env_tester_proxy_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_ENV_TESTER_PROXY_SIGNATURE);
  self->ten_env_tester = ten_env_tester;

  ten_extension_tester_t *tester = ten_env_tester->tester;
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Should not happen.");

  // Add a reference to the `ten_env_proxy` object.
  ten_env_proxy_acquire(tester->test_extension_ten_env_proxy, NULL);

  // After the `ten_env_proxy_acquire` action above, the result is stored in the
  // `ten_env_proxy` field below. This ensures that in the below
  // `ten_env_tester_on_proxy_deleted`, the `ten_env_proxy_release` operation
  // does not require setting `self->test_extension_ten_env_proxy` to a null
  // pointer (otherwise, it would feel odd to
  // `ten_env_proxy_release(tester->test_extension_ten_env_proxy)` without
  // setting it to null). This way, the last remaining `ten_env_proxy_release`
  // operation still has the `self->test_extension_ten_env_proxy` pointer
  // available for release.
  self->ten_env_proxy = tester->test_extension_ten_env_proxy;

  return self;
}

static void ten_env_tester_on_proxy_deleted(void *self_, void *arg) {
  ten_env_tester_t *ten_env_tester = self_;
  ten_env_tester_proxy_t *self = arg;
  TEN_ASSERT(
      ten_env_tester && ten_env_tester_check_integrity(ten_env_tester, true),
      "Should not happen.");
  TEN_ASSERT(self && ten_env_tester_proxy_check_integrity(self),
             "Should not happen.");

  ten_extension_tester_t *tester = ten_env_tester->tester;
  TEN_ASSERT(tester && ten_extension_tester_check_integrity(tester, true),
             "Should not happen.");

  // Release the reference to the `ten_env_proxy` object.
  ten_env_proxy_release(self->ten_env_proxy, NULL);
  self->ten_env_proxy = NULL;

  TEN_FREE(self);
}

bool ten_env_tester_proxy_release(ten_env_tester_proxy_t *self,
                                  ten_error_t *err) {
  if (!self || !ten_env_tester_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  ten_env_tester_t *ten_env_tester = self->ten_env_tester;
  TEN_ASSERT(ten_env_tester, "Should not happen.");

  int rc = ten_runloop_post_task_tail(ten_env_tester->tester->tester_runloop,
                                      ten_env_tester_on_proxy_deleted,
                                      ten_env_tester, self);
  if (rc) {
    TEN_LOGW("Failed to post task to ten_env_tester's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }

  return true;
}

bool ten_env_tester_proxy_notify(ten_env_tester_proxy_t *self,
                                 ten_env_tester_proxy_notify_func_t notify_func,
                                 void *user_data, ten_error_t *err) {
  if (!self || !notify_func || !ten_env_tester_proxy_check_integrity(self)) {
    const char *err_msg = "Invalid argument.";
    TEN_ASSERT(0, "%s", err_msg);
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT, err_msg);
    }
    return false;
  }

  ten_env_tester_t *ten_env_tester = self->ten_env_tester;
  TEN_ASSERT(ten_env_tester, "Should not happen.");

  bool result = true;

  ten_extension_tester_t *tester = ten_env_tester->tester;
  if (ten_extension_tester_thread_call_by_me(tester)) {
    notify_func(self->ten_env_tester, user_data);
  } else {
    int rc = ten_runloop_post_task_tail(tester->tester_runloop,
                                        (ten_runloop_task_func_t)notify_func,
                                        self->ten_env_tester, user_data);
    if (rc) {
      TEN_LOGW("Failed to post task to ten_env_tester's runloop: %d", rc);
    }

    result = rc == 0;
  }

  return result;
}
