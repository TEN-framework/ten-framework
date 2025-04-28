//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/signature.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/sanitizer/thread_check.h"

#define TEN_CONNECTION_SIGNATURE 0x56CFFCB7CFA81CE8U
#define TIMEOUT_FOR_CONNECTION_ALL_CLEANED 5000  // ms

typedef struct ten_app_t ten_app_t;
typedef struct ten_engine_t ten_engine_t;
typedef struct ten_remote_t ten_remote_t;
typedef struct ten_protocol_t ten_protocol_t;
typedef struct ten_connection_t ten_connection_t;

typedef void (*ten_connection_on_closed_func_t)(ten_connection_t *self,
                                                void *on_closed_data);

typedef enum TEN_CONNECTION_ATTACH_TO {
  TEN_CONNECTION_ATTACH_TO_INVALID,

  TEN_CONNECTION_ATTACH_TO_APP,

  // When the app and the engine run in different threads, and the connection
  // migration should be completed before the engine handles the messages from
  // the connection. Once the migration is completed, the messages will be
  // pushed to the queue of the engine, in other words, the connection can be
  // seen as attaching to the engine before attaching to a remote formally
  // (because that remote has not been created yet). So we use
  // 'ATTACH_TO_ENGINE' as the intermediate state to ensure that getting the
  // correct eventloop based on the 'ten_connection_t::attach_to_type' field.
  //
  // Note that the 'ten_connection_t::attach_to_type' will be 'ENGINE' or
  // 'REMOTE' when the 'ten_connection_t::migration_state' is 'DONE'. In other
  // words, the connection will use the engine's eventloop once the migration is
  // done.
  TEN_CONNECTION_ATTACH_TO_ENGINE,

  TEN_CONNECTION_ATTACH_TO_REMOTE,
} TEN_CONNECTION_ATTACH_TO;

// The accessing of the migration state variable is as follows.
//
// ================================================
//               [App Thread]     [Engine Thread]
// <INIT>           Write
// <FIRST_MSG>      Write
// <DONE>                              Write
// ================================================
//
// Refer to the comments on 'ten_connection_migrate()' to know about the race
// condition if the implementation protocol reads or writes the
// 'ten_connection_t::migration_state' field. As the race condition exists, the
// 'ten_connection_t::migration_state' can be only accessed from the TEN world
// (i.e., the app and engine thread). The implementation protocol will keep a
// mirror of 'ten_connection_t::migration_state' in its thread locally if it has
// its own thread. Refer to 'ten_protocol_asynced_t::migration_state'. In other
// words, the implementation protocol who implements from
// 'ten_protocol_integrated_t' will not keep a copy of
// 'ten_connection_t::migration_state'. It's safe to read and write
// 'ten_connection_t::migration_state' in those 'synced' protocols, as the reads
// and writes are in sequence, there is only one operation at the same time.
//
// It's safe to get the attached runloop of the connection from the external
// protocol thread, only if the 'ten_connection_t::migration_state' is 'INIT' or
// 'DONE'. As there is no messages been received by the connection when the
// 'ten_connection_t::migration_state' is 'INIT', it means that the connection
// could not be in migration, the owner thread of the connection is always the
// app thread at this time.
//
// The external protocol thread could not read the
// 'ten_connection_t::migration_state', so the implementation protocol must obey
// the following rules.
//
// - Only one message could be transferred to the TEN world _before_ the
//   migration state is completed or reset. The implementation protocol should
//   use the 'migration_state' field in its own thread locally to control the
//   message flow.
//
// - Once the migration is completed or reset in the TEN world, the
//   implementation protocol will receive an event through
//   'ten_protocol_t::on_cleaned_for_external()' callback. The implementation
//   protocol should implements this callback, and updates its own
//   'migrate_state' based on the second parameter (i.e., updates
//   'migration_state' to 'INIT' if the migration is reset in the TEN world,
//   otherwise updates to 'DONE').
//
// - Only try to retrieve the runloop of the connection (i.e., the
//   'ten_connection_t' object) from the external protocol thread when its
//   'migration_state' is 'INIT' or 'DONE'.
//
typedef enum TEN_CONNECTION_MIGRATION_STATE {
  // The initial state, it means the connection (i.e., a 'ten_connection_t'
  // object) is created, and no messages have been handled.
  TEN_CONNECTION_MIGRATION_STATE_INIT,

  // The connection is created when a client is accepted. When the connection
  // receives the first message, whether the connection needs to be migrated
  // depends on two conditions:
  //
  // - Whether the message will be sent to an TEN engine, not the TEN app. In
  //   other words, the 'graph_id' field of the dest loc of the message is not
  //   empty, or the message is a 'start_graph' cmd (the 'start_graph' cmd
  //   enables a new engine to be created).
  //
  // - Whether the TEN engine runs in its own thread.
  //
  // The above conditions are determined by the TEN app based on the message, so
  // the connection has to transfer at least one message to the TEN app. And as
  // the migration is always asynchronous, the connection could transfer only
  // one message to the TEN app before the migration is completed, otherwise the
  // migration maybe executed twice.
  //
  // So we use this state to ensure that the connection transfer one and only
  // one message to the TEN app before the migration is completed.
  TEN_CONNECTION_MIGRATION_STATE_FIRST_MSG,

  // The connection needs to be migrated, and the migration has been completed;
  // then the connection will switch to this state from the engine thread. Or,
  // the connection does not need to be migrated, and switches to this state
  // directly from the app thread.
  TEN_CONNECTION_MIGRATION_STATE_DONE,
} TEN_CONNECTION_MIGRATION_STATE;

typedef enum TEN_CONNECTION_STATE {
  TEN_CONNECTION_STATE_INIT,
  TEN_CONNECTION_STATE_CLOSING,
  TEN_CONNECTION_STATE_CLOSED,
} TEN_CONNECTION_STATE;

typedef struct ten_connection_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_string_t uri;

  TEN_CONNECTION_STATE state;

  ten_connection_on_closed_func_t on_closed;
  void *on_closed_data;

  // This is used if a connection will be attached to an engine which has its
  // own event loop.
  // TODO(Wei): Remove this block waiting mechanism, use task mechanism to
  // enable the communication between the engine and the app.
  ten_event_t *is_cleaned;

  bool duplicate;

  // TODO(Wei): This variable does not need to be an atomic type; instead, the
  // entire flow needs to be reviewed because the actual critical section
  // includes the `attached_target` below. If protection is truly needed, a
  // proper lock should be used, or there should be a mechanism ensuring that
  // these two variables are always handled in the same thread.
  ten_atomic_t attach_to;  // TEN_CONNECTION_ATTACH_TO
  union {
    ten_app_t *app;
    ten_engine_t *engine;
    ten_remote_t *remote;
  } attached_target;

  // The TEN app will create a 'connection' when a client request has been
  // accepted, and the newly created connection will be kept in the
  // 'orphan_connections' list in the app (the main reason of having this list
  // is to avoid memory leakages). If the requests from the connection are sent
  // to a TEN engine who has its own eventloop, the connection must be migrated
  // from the app to the engine before the engine handles any requests from that
  // 'connection'.
  //
  // As the migration is always asynchronous, the following situations may
  // happen simultaneously:
  //
  // - The connection receives a second request from the client side.
  //
  //   > What we have to do is, to ensure that the migration would not be
  //     executed twice. That's why we have to add the 'migration_state' field
  //     here.
  //
  // - The app closes, and the connections in the 'orphan_connections' list will
  //   be closed and destroyed.
  //
  //   > What we have to do is, to ensure that the owner of the connection is
  //     correct. After the migration, the owner of the connection will be the
  //     engine, even if the corresponding 'ten_remote_t' object has not been
  //     created yet. Otherwise, the engine may access the memory which has been
  //     freed by the app. That's what the 'attach_to_type' field does.
  //
  // - The implementation protocol who has its own thread (e.g., the http
  //   protocol) closes, and tries to send a notification to the TEN protocol
  //   through the eventloop. The implementation protocol gets the eventloop of
  //   the TEN protocol based on the 'ten_connection_t::attach_to_type' field.
  //   The implementation protocol might be closed before the migration, in
  //   other words, the 'closing' notification will be sent to the app's
  //   eventloop. But when the migration is completed in the engine thread, the
  //   owner of the connection and the TEN protocol switches to the engine. The
  //   closure of the connection might executes in the wrong thread.
  //
  //   > Briefly, the 'closing' and other events of the connection _must_ happen
  //     after the migration is completed, otherwise it's difficult to ensure
  //     that the correctness of the belonging eventloop.
  //
  // Note that connections will be also created in the 'connect_to' stage
  // (e.g., the client side sends a 'start_graph' cmd, or the extensions send a
  // 'connect_to' cmd), which always happen in the engine thread. So the
  // 'migration_state' is always 'DONE'.
  //
  // Note that this field will be only accessed in the TEN world, the
  // implementation protocol who has its own thread will keep a copy of this
  // field locally.
  //
  // Other modules except 'ten_connection_t' must call
  // 'ten_connection_get_migration_state()' and
  // 'ten_connection_set_migration_state()' to access this field to ensure the
  // thread safety.
  TEN_CONNECTION_MIGRATION_STATE migration_state;

  ten_protocol_t *protocol;
} ten_connection_t;

TEN_RUNTIME_PRIVATE_API bool ten_connection_check_integrity(
    ten_connection_t *self, bool check_thread);

TEN_RUNTIME_PRIVATE_API ten_connection_t *ten_connection_create(
    ten_protocol_t *protocol);

TEN_RUNTIME_PRIVATE_API void ten_connection_destroy(ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void ten_connection_attach_to_remote(
    ten_connection_t *self, ten_remote_t *remote);

TEN_RUNTIME_PRIVATE_API void ten_connection_attach_to_app(
    ten_connection_t *self, ten_app_t *app);

TEN_RUNTIME_PRIVATE_API TEN_CONNECTION_ATTACH_TO
ten_connection_attach_to(ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void ten_connection_send_msg(ten_connection_t *self,
                                                     ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API void ten_connection_on_msgs(ten_connection_t *self,
                                                    ten_list_t *msgs);

TEN_RUNTIME_PRIVATE_API void ten_connection_close(ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void ten_connection_set_on_closed(
    ten_connection_t *self, ten_connection_on_closed_func_t on_closed,
    void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_connection_clean(ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void ten_connection_on_protocol_closed(
    ten_protocol_t *protocol, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API void ten_connection_connect_to(
    ten_connection_t *self, const char *uri,
    void (*on_server_connected)(ten_protocol_t *, bool));

/**
 * @brief Returns the attached runloop only if the migration state is 'INIT' or
 * 'DONE', otherwise the runloop might be incorrect.
 *
 * @note Keep in mind that only one message could be transferred through this
 * connection if its migration state is 'INIT'.
 */
TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_connection_get_attached_runloop(
    ten_connection_t *self);

TEN_RUNTIME_PRIVATE_API void
ten_connection_reply_result_for_duplicate_connection(
    ten_connection_t *self, ten_shared_ptr_t *cmd_start_graph);
