//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_autoload.h"
#include "include_internal/ten_runtime/addon/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/addon/common/common.h"
#include "include_internal/ten_runtime/addon/extension/extension.h"
#include "include_internal/ten_runtime/addon/extension_group/extension_group.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/base_dir.h"
#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/app/endpoint.h"
#include "include_internal/ten_runtime/app/engine_interface.h"
#include "include_internal/ten_runtime/app/metadata.h"
#include "include_internal/ten_runtime/app/migration.h"
#include "include_internal/ten_runtime/app/predefined_graph.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension_group/builtin/builtin_extension_group.h"
#include "include_internal/ten_runtime/metadata/manifest.h"
#include "include_internal/ten_runtime/metadata/metadata.h"
#include "include_internal/ten_runtime/metadata/metadata_info.h"
#include "include_internal/ten_runtime/protocol/close.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "include_internal/ten_runtime/ten_env/log.h"
#include "include_internal/ten_runtime/ten_env/metadata_cb.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_runtime/test/test_extension.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/app/app.h"
#include "ten_runtime/ten_env/internal/on_xxx_done.h"
#include "ten_runtime/ten_env/ten_env.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

static void ten_app_adjust_and_validate_property_on_configure_done(
    ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_store_adjust_properties(&self->schema_store,
                                                    &self->property, &err);
  if (!success) {
    TEN_LOGW("Failed to adjust property type, %s.", ten_error_message(&err));
    goto done;
  }

  success = ten_schema_store_validate_properties(&self->schema_store,
                                                 &self->property, &err);
  if (!success) {
    TEN_LOGW("Invalid property, %s.", ten_error_message(&err));
    goto done;
  }

done:
  ten_error_deinit(&err);
  if (!success) {
    TEN_ASSERT(0, "Invalid property.");
  }
}

static void ten_app_start_auto_start_predefined_graph_and_trigger_on_init(
    ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(self->ten_env && ten_env_check_integrity(self->ten_env, true),
             "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool rc = ten_app_start_auto_start_predefined_graph(self, &err);
  TEN_ASSERT(rc, "Should not happen, %s.", ten_error_message(&err));

  ten_error_deinit(&err);

  // Trigger on_init.
  ten_app_on_init(self->ten_env);
}

static void ten_app_on_endpoint_protocol_created(ten_env_t *ten_env,
                                                 ten_protocol_t *protocol,
                                                 TEN_UNUSED void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (!protocol) {
    TEN_LOGE("Failed to create app endpoint protocol, FATAL ERROR.");
    ten_app_close(self, NULL);
    return;
  }

  TEN_ASSERT(ten_protocol_check_integrity(protocol, true),
             "Should not happen, %p.", protocol);

  self->endpoint_protocol = protocol;

  ten_protocol_attach_to_app(self->endpoint_protocol, self);
  ten_protocol_set_on_closed(self->endpoint_protocol,
                             ten_app_on_protocol_closed, self);

  if (!ten_app_endpoint_listen(self)) {
    TEN_LOGW("[%s] Failed to listen on endpoint protocol, %s.",
             ten_string_get_raw_str(&self->uri));
    ten_app_close(self, NULL);
    return;
  }

  ten_app_start_auto_start_predefined_graph_and_trigger_on_init(self);
}

static void ten_app_continue_run_after_load_all_extensions_if_specified(
    ten_env_t *ten_env, void *self_) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *self = (ten_app_t *)self_;
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (!ten_app_get_predefined_graphs_from_property(self)) {
    TEN_LOGW("[%s] Failed to get predefined graphs from property.",
             ten_app_get_uri(self));
    goto error;
  }

  if (!ten_string_is_equal_c_str(&self->uri, TEN_STR_LOCALHOST) &&
      !ten_string_starts_with(&self->uri, TEN_STR_CLIENT)) {
    // Create the app listening endpoint protocol if specifying one.
    bool rc = ten_addon_create_protocol_with_uri(
        self->ten_env, ten_string_get_raw_str(&self->uri),
        TEN_PROTOCOL_ROLE_LISTEN, ten_app_on_endpoint_protocol_created, NULL,
        &err);
    if (!rc) {
      TEN_LOGW("Failed to create app endpoint protocol, %s.",
               ten_error_message(&err));
      goto error;
    }
  } else {
    ten_app_start_auto_start_predefined_graph_and_trigger_on_init(self);
  }

  goto done;

error:
  ten_app_close(self, NULL);
done:
  ten_error_deinit(&err);
}

static void ten_app_continue_run_after_builtin_addons_completed(
    ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (self->preload_all_addons) {
    if (!ten_addon_load_all_extensions_from_app_base_dir(
            ten_env, ten_string_get_raw_str(&self->base_dir),
            ten_app_continue_run_after_load_all_extensions_if_specified, self,
            &err)) {
      TEN_LOGW(
          "Failed to load extensions from app base dir during preload all "
          "addons: %s",
          ten_error_message(&err));

      ten_app_close(self, NULL);
    }
  } else {
    ten_app_continue_run_after_load_all_extensions_if_specified(ten_env, self);
  }

  ten_error_deinit(&err);
}

static void ten_app_on_all_addon_loaders_created(ten_env_t *ten_env,
                                                 void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_app_t *self = (ten_app_t *)cb_data;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_app_continue_run_after_builtin_addons_completed(ten_env);
}

void ten_app_on_configure_done(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(self->loop, "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  if (self->state != TEN_APP_STATE_ON_CONFIGURE) {
    TEN_LOGI(
        "[%s] Failed to on_configure_done() because of incorrect timing: %d",
        ten_app_get_uri(self), self->state);
    ten_error_deinit(&err);
    return;
  }

  self->state = TEN_APP_STATE_ON_CONFIGURE_DONE;

  bool rc = ten_handle_manifest_info_when_on_configure_done(
      &self->manifest_info, ten_app_get_base_dir(self), &self->manifest, &err);
  if (!rc) {
    TEN_LOGW("Failed to load app manifest data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  rc = ten_handle_property_info_when_on_configure_done(
      &self->property_info, ten_app_get_base_dir(self), &self->property, &err);
  if (!rc) {
    TEN_LOGW("Failed to load app property data, FATAL ERROR.");
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (!ten_app_handle_ten_namespace_properties(self)) {
    TEN_LOGW("Failed to determine app default property.");
  }

  ten_metadata_init_schema_store(&self->manifest, &self->schema_store);
  ten_app_adjust_and_validate_property_on_configure_done(self);

  if (ten_string_is_empty(&self->uri)) {
    ten_string_set_from_c_str(&self->uri, TEN_STR_LOCALHOST);
  }

  ten_addon_manager_t *manager = ten_addon_manager_get_instance();
  TEN_ASSERT(manager, "Should not happen.");

  // There are currently two scenarios:
  //
  // 1. Single app within a process (normal case):
  //    Since addon registration (phase 2) is only needed after the app is
  //    configured, and we also want to use the TEN app's `property.json` to
  //    guide or change the TEN addon loading mechanism, we set the
  //    addon_manager to belong to this app here. Subsequently, all addon
  //    `register`/`on_init`/`on_deinit` operations will execute on the current
  //    app thread. The addon manager instance will be destroyed during _this_
  //    app's close phase to prevent memory leaks.
  //
  // 2. Multiple apps within a process (smoke/standalone tests):
  //    In smoke/standalone tests, multiple apps may exist simultaneously in a
  //    process. We ensure the addon manager instance belongs to the app with
  //    the longest lifecycle. Therefore, the first initialized app should have
  //    the longest lifecycle and will only be destroyed after all other apps
  //    are destroyed. During this period, all addon
  //    `register`/`on_init`/`on_deinit` operations will execute on this
  //    longest-lifecycle app's thread. This app is also responsible for
  //    destroying the addon manager instance to prevent memory leaks.
  bool addon_manager_belongs_to_current_app =
      ten_addon_manager_set_belonging_app_if_not_set(manager, self);
  if (!addon_manager_belongs_to_current_app) {
    // If the addon manager instance already belongs to another app, we don't
    // need to load the builtin addons and protocols again because they have
    // already been loaded.
    ten_app_continue_run_after_builtin_addons_completed(ten_env);
    return;
  }

  // Addon registration phase 1: adding a function, which will perform the
  // actual registration in the phase 2, into the `addon_manager`.
  //
  // When these two builtin addons execute their phase 2 registration depends on
  // whether they are used during runtime. If they are used, the addon-loader
  // will handle their phase 2 registration process.
  ten_addon_manager_add_builtin_extension_group(manager);
  ten_addon_manager_add_builtin_test_extension(manager);

  // Addon registration phase 1: adding a function, which will perform the
  // actual registration in the phase 2, into the `addon_manager`.
  ten_addon_load_all_protocols_and_addon_loaders_from_app_base_dir(self, &err);

  ten_addon_register_ctx_t *register_ctx = ten_addon_register_ctx_create(self);
  TEN_ASSERT(register_ctx, "Failed to allocate memory.");

  // Addon registration phase 2: actually registering the addon into the addon
  // store.
  //
  // As the current app is the addon manager's app, we can directly call the
  // register functions here because we are already on the addon manager's app
  // thread.
  //
  // Addon_loader addons and protocol addons do not implement the `on_init()`
  // function, so after the following method is called, all addon loaders and
  // protocols will be registered and added to the addon store.
  ten_addon_manager_register_all_addon_loaders(manager, register_ctx);
  ten_addon_manager_register_all_protocols(manager, register_ctx);

  ten_addon_register_ctx_destroy(register_ctx);

  // Create addon loader singleton instances.
  ten_addon_loader_addons_create_singleton_instance(
      ten_env, ten_app_on_all_addon_loaders_created, self);
}

void ten_app_on_configure(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  self->manifest_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_MANIFEST, self->ten_env);
  self->property_info =
      ten_metadata_info_create(TEN_METADATA_ATTACH_TO_PROPERTY, self->ten_env);

  self->state = TEN_APP_STATE_ON_CONFIGURE;

  if (self->on_configure) {
    self->on_configure(self, self->ten_env);
  } else {
    ten_app_on_configure_done(self->ten_env);
  }
}

void ten_app_on_init(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  self->state = TEN_APP_STATE_ON_INIT;

  if (self->on_init) {
    self->on_init(self, self->ten_env);
  } else {
    ten_app_on_init_done(self->ten_env);
  }
}

static void ten_app_on_init_done_internal(ten_app_t *self) {
  TEN_ASSERT(self && ten_app_check_integrity(self, true) && self->loop,
             "Should not happen.");
}

void ten_app_on_init_done(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (self->state != TEN_APP_STATE_ON_INIT) {
    TEN_LOGI("[%s] Failed to on_init_done() because of incorrect timing: %d",
             ten_app_get_uri(self), self->state);
    return;
  }

  self->state = TEN_APP_STATE_ON_INIT_DONE;

  ten_app_on_init_done_internal(self);
}

static void ten_app_on_all_addons_unregistered(ten_env_t *ten_env,
                                               TEN_UNUSED void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (self->on_deinit) {
    // Call the registered on_deinit callback if exists.
    self->on_deinit(self, self->ten_env);
  } else {
    ten_env_on_deinit_done(self->ten_env, NULL);
  }
}

static void ten_app_unregister_addons_after_app_close(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  if (ten_addon_manager_belongs_to_app(ten_addon_manager_get_instance(),
                                       self)) {
    ten_addon_unregister_all_and_cleanup_after_app_close(
        self->ten_env, ten_app_on_all_addons_unregistered, NULL);
  } else {
    ten_app_on_all_addons_unregistered(self->ten_env, NULL);
  }
}

void ten_app_on_deinit(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  // The world outside of TEN would do some operations after the app_run()
  // returns, so it's best to perform the on_deinit callback _before_ the
  // runloop is stopped.

  // @{
  // **Note:** The two functions below will invoke functions like `on_deinit`,
  // which may call into different language environments, such as the
  // `on_deinit` function of a Python addon. Therefore, these two functions must
  // not be called within the call flow of the C API initiated by those
  // languages. In other words, these two functions cannot be invoked within the
  // call flow of functions like `on_deinit_done`. Instead, they must be called
  // within the call flow of a purely C-native thread; otherwise, it may
  // potentially lead to a deadlock.

  // The `on_deinit` of the protocol instance needs to call the `on_deinit_done`
  // of the addon host, so this logic must be performed before unregistering the
  // protocol addons.
  if (self->endpoint_protocol) {
    ten_ref_dec_ref(&self->endpoint_protocol->ref);
  }

  // At the final stage of addon deinitialization, `ten_env_t::on_deinit_done`
  // is required, which in turn depends on the runloop. Therefore, the addon
  // deinitialization process must be performed _before_ the app's runloop
  // ends.
  ten_app_unregister_addons_after_app_close(self);
  // @}
}

bool ten_app_on_deinit_done(ten_env_t *ten_env) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true),
             "Invalid use of ten_env %p.", ten_env);

  ten_app_t *self = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_mutex_lock(self->state_lock);

  if (self->state != TEN_APP_STATE_CLOSING) {
    ten_mutex_unlock(self->state_lock);
    TEN_LOGI(
        "[%s] App cannot on_deinit_done() because of incorrect timing: %d.",
        ten_string_get_raw_str(&self->uri), self->state);
    return false;
  }

  self->state = TEN_APP_STATE_ON_DEINIT_DONE;
  ten_mutex_unlock(self->state_lock);

  // Close the ten_env so that any apis called on the ten_env will return
  // TEN_ERROR_ENV_CLOSED.
  ten_env_close(ten_env);

  if (!ten_list_is_empty(&ten_env->ten_proxy_list)) {
    // There is still the presence of ten_env_proxy, so the closing process
    // cannot continue.
    TEN_LOGI(
        "[%s] App cannot on_deinit_done() because of existed ten_env_proxy.",
        ten_string_get_raw_str(&self->uri));
    return true;
  }

  TEN_LOGI("[%s] App on_deinit_done()", ten_string_get_raw_str(&self->uri));

  ten_runloop_stop(self->loop);

  return true;
}
