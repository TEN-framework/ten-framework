//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/protocol/close.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"

#define TEN_PROTOCOL_SIGNATURE 0x72CC0E4B2E807E08U

typedef struct ten_connection_t ten_connection_t;
typedef struct ten_app_t ten_app_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_runloop_t ten_runloop_t;
typedef struct ten_addon_host_t ten_addon_host_t;
typedef struct ten_protocol_t ten_protocol_t;

typedef enum TEN_PROTOCOL_ATTACH_TO {
  TEN_PROTOCOL_ATTACH_TO_INVALID,

  // The listening protocol will be attached to a TEN app.
  TEN_PROTOCOL_ATTACH_TO_APP,

  // All protocols except the listening one will be attached to a TEN
  // connection.
  TEN_PROTOCOL_ATTACH_TO_CONNECTION,
} TEN_PROTOCOL_ATTACH_TO;

// The protocols will be created in the following scenarios:
// - A listening protocol when the app acts as a server.
// - A communication protocol when the server accepts a client from another app
//   through the graph flow.
// - A communication protocol when the server accepts a client from the external
//   world.
// - A client protocol when connecting to another app through the graph flow.
// - A client protocol when connecting to the external server.
typedef enum TEN_PROTOCOL_ROLE {
  TEN_PROTOCOL_ROLE_INVALID,

  // The listening endpoint.
  TEN_PROTOCOL_ROLE_LISTEN,

  // The protocol whose role is 'TEN_PROTOCOL_ROLE_IN_*' means that it is
  // created when the listening endpoint accepts a client. And the client might
  // be another ten app or the external world such as the browser, so we use the
  // 'INTERNAL' and 'EXTERNAL' to distinguish them.
  //
  // The reason why we need to distinguish the 'INTERNAL' and 'EXTERNAL' is that
  // the 'INTERNAL' is always created by the graph (i.e., the 'start_graph'
  // cmd). As the graph is the message flow between extensions in apps, so the
  // protocols created by the graph should be treated as persistent. In other
  // words, the life cycle of the 'INTERNAL' protocols should be equal to the
  // graph. The 'INTERNAL' protocols could _not_ be closed or destroyed until
  // the graph is closed, even if the physical connection is broken. On the
  // contrary, the 'EXTERNAL' protocols are created as needed, they should be
  // treated as temporary.
  //
  // Please keep in mind that the _IN_ in the name does _not_ mean that this
  // protocol will only process the input data. Not only the
  // 'ten_protocol_t::on_input()' function will be called in the whole life
  // cycle of the protocol whose role is 'TEN_PROTOCOL_ROLE_IN_*'. Ex: if a
  // client outside of the ten world wants to send a message to the extension in
  // the ten app, the 'ten_protocol_t::on_input()' function will be called to
  // receive the client message, but the 'ten_protocol_t::on_output()'
  // function will also be called when the extension wants to return a message
  // to the client side. A simple diagram is as follows:
  //
  //           [ external client ]
  //                 |     ^
  //     on_input()  |     | result
  //                 V     |
  //            [ ten_protocol_t ]
  //                 |     ^
  //        message  |     | on_output()
  //                 V     |
  //              [ extension ]
  TEN_PROTOCOL_ROLE_IN_INTERNAL,
  TEN_PROTOCOL_ROLE_IN_EXTERNAL,

  // The protocol whose role is 'TEN_PROTOCOL_ROLE_OUT_*' means that it is
  // created when connecting to the remote server. And the remote server might
  // be another ten app or the external server such as a nginx, so we use the
  // 'INTERNAL' and 'EXTERNAL' to distinguish them. The 'EXTERNAL' protocols are
  // always created when the engine handles the 'connect_to' cmds. So even if
  // the extension wants to connect to another ten app through the 'connect_to'
  // cmd, the created protocol will be treated as 'EXTERNAL'.
  //
  // Please keep in mind that the _OUT_ in the name does _not_ mean that this
  // protocol will only process the output data. Not only the
  // 'ten_protocol_t::on_output()' function will be called in the whole life
  // cycle of the protocol whose role is 'TEN_PROTOCOL_ROLE_OUT_*'. Ex: if an
  // extension wants to sent a message to the remote server, the
  // 'ten_protocol_t::on_output()' function will be called, but the
  // 'ten_protocol_t::on_input()' will also be called when the remote server
  // returns a result to the extension. A simple diagram is as follows:
  //
  //                [ extension ]
  //                   |     ^
  //       on_output() |     | result
  //                   V     |
  //             [ ten_protocol_t ]
  //                   |     ^
  //           message |     | on_input()
  //                   V     |
  //              [ remote server ]
  TEN_PROTOCOL_ROLE_OUT_INTERNAL,
  TEN_PROTOCOL_ROLE_OUT_EXTERNAL,

  TEN_PROTOCOL_ROLE_IN_DEFAULT = TEN_PROTOCOL_ROLE_IN_INTERNAL,
  TEN_PROTOCOL_ROLE_OUT_DEFAULT = TEN_PROTOCOL_ROLE_OUT_INTERNAL,
} TEN_PROTOCOL_ROLE;

// @{
// The interface API of 'ten_protocol_t'
typedef void (*ten_protocol_close_func_t)(ten_protocol_t *self);

typedef void (*ten_protocol_on_output_func_t)(ten_protocol_t *self,
                                              ten_list_t *output);

typedef ten_connection_t *(*ten_protocol_on_client_accepted_func_t)(
    ten_protocol_t *self, ten_protocol_t *new_protocol);

typedef void (*ten_protocol_listen_func_t)(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_client_accepted_func_t on_client_accepted);

typedef void (*ten_protocol_on_server_connected_func_t)(ten_protocol_t *self,
                                                        bool success);

typedef void (*ten_protocol_connect_to_func_t)(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_server_connected_func_t on_server_connected);

typedef void (*ten_protocol_migrate_func_t)(ten_protocol_t *self,
                                            ten_engine_t *engine,
                                            ten_connection_t *connection,
                                            ten_shared_ptr_t *cmd);

typedef void (*ten_protocol_on_migrated_func_t)(ten_protocol_t *self);

typedef void (*ten_protocol_clean_func_t)(ten_protocol_t *self);

typedef void (*ten_protocol_on_cleaned_for_internal_func_t)(
    ten_protocol_t *self);

/**
 * @brief This function will be called to notify the implementation protocol in
 * the following two scenarios:
 *
 * - The migration in the TEN world has been completed, all the resources bound
 *   to the base protocol has been cleaned during the migration.
 *
 * - The migration has not been started as the expected engine was not found.
 *   The migration state should be reset, then the connection could be checked
 *   if the migration is needed when handling the subsequent messages.
 *
 * @param is_migration_state_reset Whether the migration state has been reset.
 *
 * @note This function is always called in the ENGINE thread. So if the
 * implementation protocol runs in its own thread, this function should care
 * about the thread context switch. Refer to
 * 'ten_protocol_asynced_on_base_protocol_cleaned()'.
 */
typedef void (*ten_protocol_on_cleaned_for_external_func_t)(
    ten_protocol_t *self, bool is_migration_state_reset);
// @}

typedef enum TEN_PROTOCOL_STATE {
  TEN_PROTOCOL_STATE_INIT,
  TEN_PROTOCOL_STATE_CLOSING,
  TEN_PROTOCOL_STATE_CLOSED,
} TEN_PROTOCOL_STATE;

/**
 * @brief Base class for all protocol implementations in the TEN framework.
 *
 * All protocol implementations must inherit from `ten_protocol_t` and implement
 * required APIs such as `on_accepted`, `on_input`, and `on_output`. The
 * framework provides two standard protocol layers to accommodate different
 * threading models:
 *
 * - `ten_protocol_integrated_t`:
 *   Uses the runloop of the TEN app or engine. Suitable for protocols that
 * don't need their own thread and can operate within the main TEN execution
 * context. Example: msgpack protocol.
 *
 * - `ten_protocol_asynced_t`:
 *   Designed for protocol implementations that have their own runloop and
 * execute in a separate thread. This allows protocols with complex I/O or
 * processing requirements to operate independently without blocking the main
 * TEN thread. Example: libws_http protocol.
 *
 * The inheritance hierarchy is as follows:
 *
 *                           ten_protocol_t
 *                                  ^
 *                                  |  <== inherits
 *                                  |
 *                          +-----------------+
 *                          |                 |
 *           ten_protocol_integrated_t     ten_protocol_asynced_t
 *                      ^                             ^
 *                      |  <== inherits               |  <== inherits
 *              +---------------+             +---------------+
 *              |               |             |               |
 *            impl             impl          impl            impl
 *        (ex: msgpack)                                 (ex: libws_http)
 *
 * Protocol implementations should choose the appropriate base class based on
 * their threading requirements and interaction model with the TEN framework.
 */
typedef struct ten_protocol_t {
  ten_signature_t signature;

  // Thread check for protocol integrity verification.
  //
  // @note Thread ownership model:
  // - The base protocol must belong to the same thread as its associated
  //   connection.
  // - The implementation protocol (derived class) may run in a different
  //   thread:
  //   - For integrated protocols: Uses the same thread as the base protocol.
  //   - For asynced protocols: May use its own dedicated thread.
  //
  // Thread integrity is verified using this field to ensure thread-safety
  // when accessing protocol methods and properties.
  ten_sanitizer_thread_check_t thread_check;

  ten_ref_t ref;

  ten_addon_host_t *addon_host;

  TEN_PROTOCOL_STATE state;

  // Whether the protocol has an uncompleted async task.
  //
  // Ex: If the connect flow starts successfully, the callback will always be
  // invoked regardless of whether the connection succeeds or fails. Since the
  // callback will access the contents of the protocol instance, the whole
  // connecting flow must be treated as a single unit of execution to maintain
  // consistency. As a result, it may block the protocol's closing flow.
  bool has_uncompleted_async_task;

  ten_protocol_on_closed_func_t on_closed;
  void *on_closed_user_data;

  // This is the URI this protocol represents:
  //   - For listening protocol, this is the local URI.
  //   - For communication protocol, this is the remote URI.
  ten_string_t uri;

  // Defines the role of this protocol (server or client).
  TEN_PROTOCOL_ROLE role;

  // Specifies what this protocol is attached to (app or connection).
  // This field is thread-safe because it's immutable after assignment:
  //   - For listening protocols: Set in the app thread.
  //   - For client communication protocols: Set in the app thread when
  //     accepting clients.
  //   - For server communication protocols: Set in the engine thread when
  //     connecting to remote servers.
  //
  // IMPORTANT: If this field needs to be modified in multiple threads,
  // modifications to both 'attach_to' and 'attached_target' must happen
  // atomically as a single operation.
  TEN_PROTOCOL_ATTACH_TO attach_to;
  union {
    // The app where this protocol resides (when attach_to ==
    // TEN_PROTOCOL_ATTACH_TO_APP)
    ten_app_t *app;

    // The connection where this protocol is attached (when attach_to ==
    // TEN_PROTOCOL_ATTACH_TO_CONNECTION)
    ten_connection_t *connection;
  } attached_target;

  // Function to handle protocol closure requests.
  ten_protocol_close_func_t close;

  // Function to handle listening requests (for server protocols).
  ten_protocol_listen_func_t listen;

  // Function to handle connection requests (for client protocols).
  ten_protocol_connect_to_func_t connect_to;

  // Function to handle migration to a new runloop.
  ten_protocol_migrate_func_t migrate;

  // Function to clean up resources bound to the old runloop after migration.
  ten_protocol_clean_func_t clean;

  // TODO(Wei): Have an 'on_input' field here.

  // Function to handle outgoing TEN messages to the remote endpoint.
  ten_protocol_on_output_func_t on_output;

  // Callback function invoked when this protocol is successfully migrated to a
  // new runloop.
  ten_protocol_on_migrated_func_t on_migrated;

  // Callback functions for cleanup notification:
  // - on_cleaned_for_internal: Called for internal protocol cleanup operations.
  // - on_cleaned_for_external: Called for external protocol cleanup operations.
  ten_protocol_on_cleaned_for_internal_func_t on_cleaned_for_internal;
  ten_protocol_on_cleaned_for_external_func_t on_cleaned_for_external;

  // @{
  // Fields for storing incoming messages (input data).
  //
  // NOTE: The 'in_lock' field is currently not necessary:
  // - For integrated protocols: All reads/writes to 'in_msgs' occur in the same
  //   thread, so accesses are sequential even during migration.
  // - For asynced protocols: All 'in_msgs' are transferred through runloop
  //   tasks, which handles thread synchronization.
  ten_mutex_t *in_lock;
  ten_list_t in_msgs;
  // @}

  // @{
  // Fields for storing outgoing messages (output data).
  // The mutex protects concurrent access to the output message queue.
  ten_mutex_t *out_lock;
  ten_list_t out_msgs;
  // @}
} ten_protocol_t;

TEN_RUNTIME_PRIVATE_API void ten_protocol_listen(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_client_accepted_func_t on_client_accepted);

TEN_RUNTIME_PRIVATE_API void ten_protocol_connect_to(
    ten_protocol_t *self, const char *uri,
    ten_protocol_on_server_connected_func_t on_server_connected);

TEN_RUNTIME_PRIVATE_API void ten_protocol_migrate(
    ten_protocol_t *self, ten_engine_t *engine, ten_connection_t *connection,
    ten_shared_ptr_t *cmd, ten_protocol_on_migrated_func_t on_migrated);

TEN_RUNTIME_PRIVATE_API void ten_protocol_clean(
    ten_protocol_t *self,
    ten_protocol_on_cleaned_for_internal_func_t on_cleaned_for_internal);

TEN_RUNTIME_PRIVATE_API void ten_protocol_update_belonging_thread_on_cleaned(
    ten_protocol_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_attach_to_connection(
    ten_protocol_t *self, ten_connection_t *connection);

/**
 * @brief Try to send one message to check if the connection needs to be
 * migrated when handling the first message, or just send one message after the
 * migration is completed.
 */
TEN_RUNTIME_PRIVATE_API void ten_protocol_on_input(ten_protocol_t *self,
                                                   ten_shared_ptr_t *msg);

/**
 * @brief Send messages in batch after the migration is completed.
 *
 * @note The caller side must be responsible to ensure that the migration has
 * been completed.
 */
TEN_RUNTIME_PRIVATE_API void ten_protocol_on_inputs(ten_protocol_t *self,
                                                    ten_list_t *msgs);

TEN_RUNTIME_PRIVATE_API ten_string_t *ten_protocol_uri_to_transport_uri(
    ten_protocol_t *self, const char *uri);

TEN_RUNTIME_PRIVATE_API void ten_protocol_set_uri(ten_protocol_t *self,
                                                  ten_string_t *uri);

TEN_RUNTIME_PRIVATE_API const char *ten_protocol_get_uri(ten_protocol_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_set_addon(
    ten_protocol_t *self, ten_addon_host_t *addon_host);

TEN_RUNTIME_API TEN_PROTOCOL_ATTACH_TO
ten_protocol_attach_to(ten_protocol_t *self);

TEN_RUNTIME_API bool ten_protocol_check_integrity(ten_protocol_t *self,
                                                  bool check_thread);

TEN_RUNTIME_API void ten_protocol_init(
    ten_protocol_t *self, const char *name, ten_protocol_close_func_t close,
    ten_protocol_on_output_func_t on_output, ten_protocol_listen_func_t listen,
    ten_protocol_connect_to_func_t connect_to,
    ten_protocol_migrate_func_t migrate, ten_protocol_clean_func_t clean);

TEN_RUNTIME_API void ten_protocol_deinit(ten_protocol_t *self);

TEN_RUNTIME_PRIVATE_API void ten_protocol_attach_to_app(ten_protocol_t *self,
                                                        ten_app_t *app);

TEN_RUNTIME_PRIVATE_API void ten_protocol_send_msg(ten_protocol_t *self,
                                                   ten_shared_ptr_t *msg);

/**
 * @return NULL if the protocol attaches to a connection who is in migration.
 */
TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_protocol_get_attached_runloop(
    ten_protocol_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_protocol_role_is_communication(
    ten_protocol_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_protocol_role_is_listening(
    ten_protocol_t *self);
