//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/protocol/protocol.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/migration.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/close.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/uri.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/thread_check.h"
#include "ten_utils/value/value_object.h"

bool ten_protocol_check_integrity(ten_protocol_t *self, bool check_thread) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_PROTOCOL_SIGNATURE) {
    return false;
  }

  if (check_thread) {
    return ten_sanitizer_thread_check_do_check(&self->thread_check);
  }

  return true;
}

// The life cycle of 'protocol' is managed by a ten_ref_t, so the 'destroy'
// function of protocol is declared as 'static' to avoid wrong usage of the
// 'destroy' function.
static void ten_protocol_destroy(ten_protocol_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The belonging thread of the 'protocol' is
                 // ended when this function is called, so we can not check
                 // thread integrity here.
                 ten_protocol_check_integrity(self, false),
             "Invalid argument.");
  TEN_ASSERT(self->state == TEN_PROTOCOL_STATE_CLOSED,
             "Protocol should be closed first before been destroyed.");

  self->addon_host->addon->on_destroy_instance(
      self->addon_host->addon, self->addon_host->ten_env, self, NULL);
}

static void ten_protocol_on_end_of_life(TEN_UNUSED ten_ref_t *ref,
                                        void *supervisor) {
  ten_protocol_t *self = (ten_protocol_t *)supervisor;
  TEN_ASSERT(self && ten_protocol_check_integrity(self, false),
             "Should not happen.");
  TEN_ASSERT(self->state == TEN_PROTOCOL_STATE_CLOSED,
             "The protocol should be closed first before being destroyed.");

  ten_ref_deinit(&self->ref);

  ten_protocol_destroy(self);
}

void ten_protocol_init(ten_protocol_t *self, const char *name,
                       ten_protocol_close_func_t close,
                       ten_protocol_on_output_func_t on_output,
                       ten_protocol_listen_func_t listen,
                       ten_protocol_connect_to_func_t connect_to,
                       ten_protocol_migrate_func_t migrate,
                       ten_protocol_clean_func_t clean) {
  TEN_ASSERT(self && name, "Should not happen.");

  ten_signature_set(&self->signature, (ten_signature_t)TEN_PROTOCOL_SIGNATURE);

  ten_sanitizer_thread_check_init_with_current_thread(&self->thread_check);

  self->addon_host = NULL;
  self->state = TEN_PROTOCOL_STATE_INIT;
  self->has_uncompleted_async_task = false;

  self->close = close;

  self->on_output = on_output;

  self->listen = listen;
  self->connect_to = connect_to;

  self->migrate = migrate;
  self->on_migrated = NULL;

  self->clean = clean;
  self->on_cleaned_for_internal = NULL;

  self->on_cleaned_for_external = NULL;

  TEN_STRING_INIT(self->uri);

  self->attach_to = TEN_PROTOCOL_ATTACH_TO_INVALID;
  self->attached_target.app = NULL;
  self->attached_target.connection = NULL;

  self->in_lock = ten_mutex_create();
  TEN_ASSERT(self->in_lock, "Should not happen.");
  self->out_lock = ten_mutex_create();
  TEN_ASSERT(self->out_lock, "Should not happen.");

  self->role = TEN_PROTOCOL_ROLE_INVALID;

  ten_list_init(&self->in_msgs);
  ten_list_init(&self->out_msgs);

  ten_ref_init(&self->ref, self, ten_protocol_on_end_of_life);
}

void ten_protocol_deinit(ten_protocol_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 // thread-check: The belonging thread of the 'protocol' is
                 // ended when this function is called, so we can not check
                 // thread integrity here.
                 ten_protocol_check_integrity(self, false),
             "Should not happen.");

  ten_signature_set(&self->signature, 0);

  self->attach_to = TEN_PROTOCOL_ATTACH_TO_INVALID;
  self->attached_target.app = NULL;
  self->attached_target.connection = NULL;

  ten_string_deinit(&self->uri);

  ten_mutex_destroy(self->in_lock);
  ten_list_clear(&self->in_msgs);

  ten_mutex_destroy(self->out_lock);
  ten_list_clear(&self->out_msgs);

  if (self->addon_host) {
    // Since the protocol has already been destroyed, there is no need to
    // release its resources through the corresponding addon anymore. Therefore,
    // decrement the reference count of the corresponding addon.
    ten_ref_dec_ref(&self->addon_host->ref);
    self->addon_host = NULL;
  }

  ten_sanitizer_thread_check_deinit(&self->thread_check);
}

void ten_protocol_listen(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_client_accepted_func_t on_client_accepted) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_listening(self),
             "Only the listening protocol could listen.");
  TEN_ASSERT(self->listen && uri && on_client_accepted, "Should not happen.");

  TEN_ASSERT(self->attach_to == TEN_PROTOCOL_ATTACH_TO_APP,
             "Should not happen.");

  ten_app_t *app = self->attached_target.app;
  TEN_ASSERT(app && ten_app_check_integrity(app, true),
             "Access across threads.");

  self->listen(self, uri, on_client_accepted);
}

void ten_protocol_attach_to_app(ten_protocol_t *self, ten_app_t *app) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  self->attach_to = TEN_PROTOCOL_ATTACH_TO_APP;
  self->attached_target.app = app;
}

void ten_protocol_attach_to_connection(ten_protocol_t *self,
                                       ten_connection_t *connection) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  self->attach_to = TEN_PROTOCOL_ATTACH_TO_CONNECTION;
  self->attached_target.connection = connection;

  ten_protocol_set_on_closed(self, ten_connection_on_protocol_closed,
                             connection);
}

TEN_PROTOCOL_ATTACH_TO
ten_protocol_attach_to(ten_protocol_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function will be called from the implementation protocol
  // who has its own thread, and the modification and reading of
  // 'attach_to_type' field is time-staggered.
  TEN_ASSERT(self && ten_protocol_check_integrity(self, false),
             "Invalid argument.");

  return self->attach_to;
}

void ten_protocol_on_input(ten_protocol_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");

  if (self->state == TEN_PROTOCOL_STATE_CLOSING) {
    TEN_LOGD("Protocol is closing, do not handle msgs.");
    return;
  }

  TEN_ASSERT(ten_protocol_role_is_communication(self),
             "Only the protocols of the communication type should receive TEN "
             "messages.");
  TEN_ASSERT(self->attach_to == TEN_PROTOCOL_ATTACH_TO_CONNECTION,
             "The protocol should have already been attached to a connection.");

  ten_connection_t *connection = self->attached_target.connection;
  TEN_ASSERT(connection,
             "The protocol should have already been attached to a connection.");

  TEN_CONNECTION_MIGRATION_STATE migration_state =
      ten_connection_get_migration_state(connection);
  TEN_ASSERT(migration_state == TEN_CONNECTION_MIGRATION_STATE_INIT ||
                 migration_state == TEN_CONNECTION_MIGRATION_STATE_DONE,
             "The protocol only can handle the input messages when the "
             "migration has not started yet or has been completed.");

  if (migration_state == TEN_CONNECTION_MIGRATION_STATE_INIT) {
    ten_connection_set_migration_state(
        connection, TEN_CONNECTION_MIGRATION_STATE_FIRST_MSG);
  }

  ten_list_t msgs = TEN_LIST_INIT_VAL;
  ten_list_push_smart_ptr_back(&msgs, msg);

  ten_connection_on_msgs(connection, &msgs);

  ten_list_clear(&msgs);
}

void ten_protocol_on_inputs(ten_protocol_t *self, ten_list_t *msgs) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msgs, "Should not happen.");

  if (self->state == TEN_PROTOCOL_STATE_CLOSING) {
    TEN_LOGD("Protocol is closing, do not handle msgs.");
    return;
  }

  TEN_ASSERT(ten_protocol_role_is_communication(self),
             "Only the protocols of the communication type should receive TEN "
             "messages.");
  TEN_ASSERT(self->attach_to == TEN_PROTOCOL_ATTACH_TO_CONNECTION,
             "The protocol should have already been attached to a connection.");

  ten_connection_t *connection = self->attached_target.connection;
  TEN_ASSERT(connection,
             "The protocol should have already been attached to a connection.");
  TEN_ASSERT(ten_connection_get_migration_state(connection) ==
                 TEN_CONNECTION_MIGRATION_STATE_DONE,
             "The connection migration must be completed when batch handling "
             "messages.");

  ten_connection_on_msgs(connection, msgs);
}

void ten_protocol_send_msg(ten_protocol_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msg, "Should not happen.");

  if (self->state == TEN_PROTOCOL_STATE_CLOSING) {
    TEN_LOGD("Protocol is closing, do not send msgs.");
    return;
  }

  if (self->on_output) {
    ten_list_t msgs = TEN_LIST_INIT_VAL;

    ten_listnode_t *node = ten_smart_ptr_listnode_create(msg);
    ten_list_push_back(&msgs, node);

    self->on_output(self, &msgs);
  }
}

void ten_protocol_connect_to(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_server_connected_func_t on_server_connected) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(self),
             "Only the communication protocol could connect to remote.");
  TEN_ASSERT(uri, "Should not happen.");
  TEN_ASSERT(on_server_connected, "Should not happen.");

  if (self->attach_to == TEN_PROTOCOL_ATTACH_TO_CONNECTION &&
      ten_connection_attach_to(self->attached_target.connection) ==
          TEN_CONNECTION_ATTACH_TO_REMOTE) {
    TEN_ASSERT(
        ten_engine_check_integrity(
            self->attached_target.connection->attached_target.remote->engine,
            true),
        "Should not happen.");
  }

  if (self->connect_to) {
    self->connect_to(self, uri, on_server_connected);
  } else {
    // The protocol doesn't implement the 'connect_to' function, so the
    // 'on_server_connected' callback is called directly.
    on_server_connected(self, false);
  }
}

void ten_protocol_migrate(ten_protocol_t *self, ten_engine_t *engine,
                          ten_connection_t *connection, ten_shared_ptr_t *cmd,
                          ten_protocol_on_migrated_func_t on_migrated) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  // Call in the app thread.
  TEN_ASSERT(ten_app_check_integrity(engine->app, true), "Should not happen.");

  self->on_migrated = on_migrated;
  if (self->migrate) {
    self->migrate(self, engine, connection, cmd);
  }
}

void ten_protocol_clean(
    ten_protocol_t *self,
    ten_protocol_on_cleaned_for_internal_func_t on_cleaned_for_internal) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(on_cleaned_for_internal, "Should not happen.");
  TEN_ASSERT(self->attached_target.connection &&
                 ten_connection_attach_to(self->attached_target.connection) ==
                     TEN_CONNECTION_ATTACH_TO_APP,
             "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(
                 self->attached_target.connection->attached_target.app, true),
             "Should not happen.");

  self->on_cleaned_for_internal = on_cleaned_for_internal;
  if (self->clean) {
    self->clean(self);
  } else {
    // The actual protocol implementation doesn't need to do cleanup during the
    // migration, so the 'on_cleaned_for_internal' callback is called now.
    self->on_cleaned_for_internal(self);
  }
}

void ten_protocol_update_belonging_thread_on_cleaned(ten_protocol_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_sanitizer_thread_check_set_belonging_thread_to_current_thread(
      &self->thread_check);
  TEN_ASSERT(ten_protocol_check_integrity(self, true),
             "Access across threads.");
}

void ten_protocol_set_addon(ten_protocol_t *self,
                            ten_addon_host_t *addon_host) {
  TEN_ASSERT(self, "Should not happen.");
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: in the case of JS binding, the extension group would
  // initially created in the JS main thread, and and engine thread will
  // call this function. However, these operations are all occurred
  // before the whole extension system is running, so it's thread safe.
  TEN_ASSERT(ten_protocol_check_integrity(self, false), "Should not happen.");

  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host), "Should not happen.");

  // Since the extension requires the corresponding addon to release
  // its resources, therefore, hold on to a reference count of the corresponding
  // addon.
  TEN_ASSERT(!self->addon_host, "Should not happen.");
  self->addon_host = addon_host;
  ten_ref_inc_ref(&addon_host->ref);
}

ten_string_t *ten_protocol_uri_to_transport_uri(ten_protocol_t *self,
                                                const char *uri) {
  TEN_ASSERT(uri && strlen(uri), "Should not happen.");

  ten_string_t *protocol_str = ten_uri_get_protocol(uri);
  ten_string_t *host_str = ten_uri_get_host(uri);
  uint16_t port = ten_uri_get_port(uri);

  ten_addon_host_t *addon_host = self->addon_host;
  TEN_ASSERT(addon_host, "Should not happen.");
  TEN_ASSERT(ten_addon_host_check_integrity(addon_host), "Should not happen.");

  const char *transport_type = ten_value_object_peek_string(
      &addon_host->manifest, TEN_STR_TRANSPORT_TYPE);
  if (!transport_type) {
    TEN_LOGW("No transport type found in the manifest, use the default: %s.",
             TEN_STR_TCP);
    transport_type = TEN_STR_TCP;
  }

  ten_string_t *transport_uri = ten_string_create_formatted(
      "%s://%s:%d/", transport_type, ten_string_get_raw_str(host_str), port);

  ten_string_destroy(protocol_str);
  ten_string_destroy(host_str);

  return transport_uri;
}

ten_runloop_t *ten_protocol_get_attached_runloop(ten_protocol_t *self) {
  TEN_ASSERT(self &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_protocol_check_integrity(self, false),
             "This function is intended to be called in different threads.");

  switch (self->attach_to) {
  case TEN_PROTOCOL_ATTACH_TO_APP:
    return ten_app_get_attached_runloop(self->attached_target.app);
  case TEN_PROTOCOL_ATTACH_TO_CONNECTION:
    return ten_connection_get_attached_runloop(
        self->attached_target.connection);

  default:
    TEN_ASSERT(0, "Should not happen.");
    break;
  }

  return NULL;
}

void ten_protocol_set_uri(ten_protocol_t *self, ten_string_t *uri) {
  TEN_ASSERT(self && uri, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true),
             "Access across threads.");

  ten_string_copy(&self->uri, uri);
}

const char *ten_protocol_get_uri(ten_protocol_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true),
             "Access across threads.");

  return ten_string_get_raw_str(&self->uri);
}

bool ten_protocol_role_is_communication(ten_protocol_t *self) {
  TEN_ASSERT(self && ten_protocol_check_integrity(self, true),
             "Access across threads.");

  return self->role > TEN_PROTOCOL_ROLE_LISTEN;
}

bool ten_protocol_role_is_listening(ten_protocol_t *self) {
  TEN_ASSERT(self && ten_protocol_check_integrity(self, true),
             "Access across threads.");

  return self->role == TEN_PROTOCOL_ROLE_LISTEN;
}
