//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/binding/nodejs/common/tsfn.h"

#include "include_internal/ten_runtime/binding/nodejs/common/common.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

static ten_nodejs_tsfn_t *ten_nodejs_tsfn_create_empty(void) {
  ten_nodejs_tsfn_t *self = TEN_MALLOC(sizeof(ten_nodejs_tsfn_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_signature_set(&self->signature, TEN_NODEJS_THREADSAFE_FUNCTION_SIGNATURE);
  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  TEN_STRING_INIT(self->name);
  self->js_func_ref = NULL;
  self->tsfn = NULL;
  self->lock = ten_mutex_create();

  return self;
}

/**
 * @brief This function will be called in the JS main thread.
 */
static void ten_nodejs_tsfn_finalize(napi_env env, void *finalize_data,
                                     TEN_UNUSED void *finalize_hint) {
  TEN_ASSERT(env, "Should not happen.");

  ten_nodejs_tsfn_t *tsfn_bridge = finalize_data;
  TEN_ASSERT(tsfn_bridge && ten_nodejs_tsfn_check_integrity(tsfn_bridge, true),
             "Should not happen.");

  TEN_LOGD("TSFN %s is finalized.", ten_string_get_raw_str(&tsfn_bridge->name));

  // The tsfn would be accessed from the native part, so we need to protect the
  // operations of it.
  ten_mutex_lock(tsfn_bridge->lock);
  // Indicate that the tsfn is disappear.
  tsfn_bridge->tsfn = NULL;
  ten_mutex_unlock(tsfn_bridge->lock);

  // Release the reference to the JS function which this tsfn points to.
  TEN_ASSERT(tsfn_bridge->js_func_ref, "Should not happen.");
  uint32_t js_func_ref_cnt = 0;
  napi_status status =
      napi_reference_unref(env, tsfn_bridge->js_func_ref, &js_func_ref_cnt);
  ASSERT_IF_NAPI_FAIL(
      status == napi_ok,
      "Failed to release JS func ref pointed by TSFN \"%s\" (%d)",
      ten_string_get_raw_str(&tsfn_bridge->name), js_func_ref_cnt);
  TEN_ASSERT(js_func_ref_cnt == 0,
             "The reference count, to the JS func, hold by tsfn should be 1.");

  TEN_LOGD(
      "Release JS func ref pointed by TSFN \"%s\", its new ref count is %d",
      ten_string_get_raw_str(&tsfn_bridge->name), js_func_ref_cnt);

  status = napi_delete_reference(env, tsfn_bridge->js_func_ref);
  ASSERT_IF_NAPI_FAIL(status == napi_ok,
                      "Failed to delete JS func ref pointed by TSFN \"%s\"",
                      ten_string_get_raw_str(&tsfn_bridge->name));

  // Indicate that the JS tsfn has destroyed.
  ten_nodejs_tsfn_dec_rc(tsfn_bridge);
}

static void ten_nodejs_tsfn_destroy(ten_nodejs_tsfn_t *self) {
  TEN_ASSERT(
      self &&
          // TEN_NOLINTNEXTLINE(thread-check)
          // thread-check: if reaching here, it means all the user of the tsfn
          // has ended, so it's safe to call this function in any threads.
          ten_nodejs_tsfn_check_integrity(self, false) &&
          // Before being destroyed, the TSFN should have already been
          // finalized.
          !self->tsfn,
      "Should not happen.");

  ten_string_deinit(&self->name);
  if (self->lock) {
    ten_mutex_destroy(self->lock);
    self->lock = NULL;
  }

  ten_sanitizer_thread_check_deinit(&self->thread_check);

  TEN_FREE(self);
}

static void ten_nodejs_tsfn_on_end_of_life(TEN_UNUSED ten_ref_t *ref,
                                           void *self_) {
  ten_nodejs_tsfn_t *self = (ten_nodejs_tsfn_t *)self_;

  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: The belonging thread of the 'client' is ended when this
  // function is called, so we can not check thread integrity here.
  TEN_ASSERT(self && ten_nodejs_tsfn_check_integrity(self, false),
             "Invalid argument.");

  ten_ref_deinit(&self->ref);
  ten_nodejs_tsfn_destroy(self);
}

bool ten_nodejs_tsfn_check_integrity(ten_nodejs_tsfn_t *self,
                                     bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      TEN_NODEJS_THREADSAFE_FUNCTION_SIGNATURE) {
    return false;
  }

  if (check_thread &&
      !ten_sanitizer_thread_check_do_check(&self->thread_check)) {
    return false;
  }

  return true;
}

ten_nodejs_tsfn_t *ten_nodejs_tsfn_create(
    napi_env env, const char *name, napi_value js_func,
    napi_threadsafe_function_call_js tsfn_proxy_func) {
  TEN_ASSERT(env && name && js_func && tsfn_proxy_func, "Should not happen.");

  ten_nodejs_tsfn_t *self = ten_nodejs_tsfn_create_empty();

  napi_status status = napi_ok;

  // Create a JS reference to keep the JS function alive.
  status = napi_create_reference(env, js_func, 1 /* Initial reference count */,
                                 &self->js_func_ref);
  ASSERT_IF_NAPI_FAIL(status == napi_ok && self->js_func_ref != NULL,
                      "Failed to create reference to JS function %p: %d",
                      js_func, status);

  // Create a name to represent this work. This is must, otherwise,
  // 'napi_create_threadsafe_function' will fail because of this.
  napi_value work_name = NULL;
  status = napi_create_string_utf8(env, name, strlen(name), &work_name);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create JS string: %d",
                      status);

  ten_string_init_formatted(&self->name, "%s", name);

  // Create a TSFN for the corresponding javascript function 'js_func'.
  status = napi_create_threadsafe_function(
      env, js_func, NULL, work_name, 0, 1 /* Initial thread count */, self,
      ten_nodejs_tsfn_finalize, NULL, tsfn_proxy_func, &self->tsfn);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to create TSFN: %d", status);

  // Indicate that the JS part takes one ownership of this tsfn bridge.
  ten_ref_init(&self->ref, self, ten_nodejs_tsfn_on_end_of_life);

  return self;
}

void ten_nodejs_tsfn_inc_rc(ten_nodejs_tsfn_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is meant to be called in any
                 // threads, and all operations in this function is thread-safe.
                 ten_nodejs_tsfn_check_integrity(self, false),
             "Should not happen.");

  ten_ref_inc_ref(&self->ref);
}

void ten_nodejs_tsfn_dec_rc(ten_nodejs_tsfn_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: this function is meant to be called in any
                 // threads, and all operations in this function is thread-safe.
                 ten_nodejs_tsfn_check_integrity(self, false),
             "Should not happen.");

  ten_ref_dec_ref(&self->ref);
}

bool ten_nodejs_tsfn_invoke(ten_nodejs_tsfn_t *self, void *data) {
  TEN_ASSERT(self, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: this function is meant to be called in all threads.
  TEN_ASSERT(ten_nodejs_tsfn_check_integrity(self, false),
             "Should not happen.");

  bool result = true;

  // Because rte_nodejs_tsfn_t would be accessed in the JS main
  // thread at any time, so we need a locking here.
  ten_mutex_lock(self->lock);

  if (self->tsfn == NULL) {
    TEN_LOGW("Failed to call tsfn %s, because it has been finalized.",
             ten_string_get_raw_str(&self->name));

    result = false;
    goto done;
  }

  napi_status status =
      napi_call_threadsafe_function(self->tsfn, data, napi_tsfn_blocking);
  if (status != napi_ok) {
    if (status == napi_closing) {
      TEN_LOGE("Failed to call a destroyed JS thread-safe function %s.",
               ten_string_get_raw_str(&self->name));
    } else {
      TEN_LOGE("Failed to call a JS thread-safe function %s: status: %d",
               ten_string_get_raw_str(&self->name), status);
    }

    result = false;
    goto done;
  }

done:
  ten_mutex_unlock(self->lock);
  return result;
}

void ten_nodejs_tsfn_release(ten_nodejs_tsfn_t *self) {
  TEN_ASSERT(self && ten_nodejs_tsfn_check_integrity(self, true),
             "Should not happen.");

  TEN_LOGD("Release TSFN \"%s\" (%p)", ten_string_get_raw_str(&self->name),
           self->tsfn);

  // 'releasing' the TSFN, so that it can be garbage collected.
  napi_status status =
      napi_release_threadsafe_function(self->tsfn, napi_tsfn_abort);
  ASSERT_IF_NAPI_FAIL(status == napi_ok, "Failed to release TSFN: %d", status);
}
