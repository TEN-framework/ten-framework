//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/protocol/integrated/protocol_integrated.h"

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/addon/protocol/protocol.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/app/migration.h"
#include "include_internal/ten_runtime/connection/connection.h"
#include "include_internal/ten_runtime/connection/migration.h"
#include "include_internal/ten_runtime/engine/engine.h"
#include "include_internal/ten_runtime/engine/internal/migration.h"
#include "include_internal/ten_runtime/engine/internal/thread.h"
#include "include_internal/ten_runtime/engine/msg_interface/common.h"
#include "include_internal/ten_runtime/msg/cmd_base/cmd_base.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "include_internal/ten_runtime/protocol/close.h"
#include "include_internal/ten_runtime/protocol/integrated/close.h"
#include "include_internal/ten_runtime/protocol/protocol.h"
#include "include_internal/ten_runtime/remote/remote.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/container/list.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/io/stream.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/ref.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

#if defined(_DEBUG)
#include "ten_utils/lib/time.h"
#endif

static void ten_protocol_close_task(void *self_, TEN_UNUSED void *arg) {
  ten_protocol_t *self = (ten_protocol_t *)self_;
  TEN_ASSERT(self && ten_protocol_check_integrity(self, true),
             "Access across threads.");

  ten_protocol_close(self);

  ten_ref_dec_ref(&self->ref);
}

static void ten_protocol_on_inputs_based_on_migration_state(
    ten_protocol_t *self, ten_list_t *msgs) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");
  TEN_ASSERT(msgs, "Should not happen.");
  TEN_ASSERT(ten_protocol_attach_to(self) == TEN_PROTOCOL_ATTACH_TO_CONNECTION,
             "Should not happen.");

  ten_connection_t *connection = self->attached_target.connection;
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  // The stream will be frozen before the migration, and this function is only
  // called when the integrated protocol retrieves data from the stream. So the
  // 'ten_connection_t::migration_state' here is only read or written in one
  // thread at any time.
  //
  // - Before the migration, this function is called in the app thread, the
  //   'ten_connection_t::migration_state' will be read and written in the app
  //   thread.
  //
  // - No messages during the migration, which means this function will not be
  //   called once the migration is started.
  //
  // - When the migration is completed, this function is always called in the
  //   engine thread, and the messages will be transferred to the corresponding
  //   remote (i.e., the 'ten_remote_t' object) directly, the app thread will
  //   not access the 'migration_state' any more.
  TEN_CONNECTION_MIGRATION_STATE migration_state =
      ten_connection_get_migration_state(connection);

  if (migration_state == TEN_CONNECTION_MIGRATION_STATE_INIT) {
    {
      // Feed the very first message to the TEN world.

      ten_listnode_t *first_msg_node = ten_list_pop_front(msgs);
      ten_shared_ptr_t *first_msg = ten_smart_ptr_listnode_get(first_msg_node);
      TEN_ASSERT(first_msg && ten_msg_check_integrity(first_msg),
                 "Invalid argument.");

      ten_protocol_on_input(self, first_msg);
      ten_listnode_destroy(first_msg_node);
    }

    // Cache the rest messages.
    if (!ten_list_is_empty(msgs)) {
      // TODO(Liu): The 'in_msgs' queue only accessed by one thread at any time,
      // does it need to be protected by 'in_lock'?
      TEN_DO_WITH_MUTEX_LOCK(self->in_lock,
                             { ten_list_concat(&self->in_msgs, msgs); });
    }
  } else if (migration_state == TEN_CONNECTION_MIGRATION_STATE_DONE) {
    ten_protocol_on_inputs(self, msgs);
  } else {
    TEN_ASSERT(
        0, "The stream should be frozen before the migration is completed.");
  }
}

static void ten_stream_on_data(ten_stream_t *stream, void *data, int size) {
  TEN_ASSERT(stream, "Should not happen.");

  ten_protocol_integrated_t *protocol =
      (ten_protocol_integrated_t *)stream->user_data;
  TEN_ASSERT(protocol, "Invalid argument.");

  ten_protocol_t *base_protocol = &protocol->base;
  TEN_ASSERT(ten_protocol_check_integrity(base_protocol, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_attach_to(base_protocol) ==
                 TEN_PROTOCOL_ATTACH_TO_CONNECTION,
             "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(base_protocol),
             "Should not happen.");

  ten_connection_t *connection = base_protocol->attached_target.connection;
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  if (size < 0) {
    // Something unexpected happened, close the protocol.
    TEN_LOGD("Failed to receive data, close the protocol: %d", size);

    // This branch means that the client side closes the physical connection
    // first, and then the corresponding protocol will be closed. An example of
    // this case is 'ExtensionTest.BasicMultiAppCloseThroughEngine'.
    //
    // But the closing of the protocol should always be an _async_ operation.
    // Take 'ExtensionTest.BasicMultiAppCloseThroughEngine' as an example, the
    // reason is as follows:
    //
    // 1. The client sends a custom cmd to the engine, and the corresponding
    //    remote (i.e., a 'ten_remote_t' object) is created, then the connection
    //    (i.e., a 'ten_connection_t' object) attached to the remote.
    //
    // 2. The client sends a 'close_app' cmd through the same connection, the
    //    cmd will be handled by the engine as the connection attaches to the
    //    remote now. And then the 'close_app' cmd will be submitted to the
    //    app's message queue as an eventloop task.
    //
    // 3. Then the client closes the physical connection, this function will be
    //    called from the app thread (because the engine doesn't have its own
    //    engine thread in this case). If the protocol (i.e., the
    //    'ten_protocol_t' object) is closed synchronized, the connection maybe
    //    destroyed before the 'close_app' cmd (in step 2) is executed.
    //
    // So the closing of the protocol should always be an _async_ operation, as
    // the stream is being closed and there is no chance to receive data from
    // the stream. The 'ten_protocol_close_task_()' will the last task related
    // to the connection.
    ten_ref_inc_ref(&base_protocol->ref);

    int rc = ten_runloop_post_task_tail(
        ten_connection_get_attached_runloop(connection),
        ten_protocol_close_task, base_protocol, NULL);
    if (rc) {
      TEN_LOGW("Failed to post task to connection's runloop: %d", rc);
      ten_ref_dec_ref(&base_protocol->ref);

      TEN_ASSERT(0, "Should not happen.");
    }
  } else if (size > 0) {
    ten_list_t msgs = TEN_LIST_INIT_VAL;

    protocol->on_input(
        protocol, TEN_BUF_STATIC_INIT_WITH_DATA_UNOWNED(data, size), &msgs);

    ten_protocol_on_inputs_based_on_migration_state(base_protocol, &msgs);

    ten_list_clear(&msgs);
  }
}

static void ten_protocol_integrated_send_buf(ten_protocol_integrated_t *self,
                                             ten_buf_t buf) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");

  TEN_UNUSED int rc =
      ten_stream_send(self->role_facility.communication_stream,
                      (const char *)buf.data, buf.content_size, buf.data);
  TEN_ASSERT(!rc, "ten_stream_send() failed: %d", rc);
}

static void ten_protocol_integrated_on_output(ten_protocol_integrated_t *self) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(&self->base),
             "Should not happen.");

  if (self->base.state == TEN_PROTOCOL_STATE_CLOSING) {
    TEN_LOGD("Protocol is closing, do not actually send msgs.");
    return;
  }

  ten_list_t out_msgs_ = TEN_LIST_INIT_VAL;

  TEN_UNUSED int rc = ten_mutex_lock(self->base.out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_list_swap(&out_msgs_, &self->base.out_msgs);

  rc = ten_mutex_unlock(self->base.out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  if (ten_list_size(&out_msgs_)) {
    ten_buf_t output_buf = TEN_BUF_STATIC_INIT_UNOWNED;

    if (self->on_output) {
      output_buf = self->on_output(self, &out_msgs_);
    }

    if (output_buf.data) {
      // If the connection is a TCP channel, and is reset by peer, the following
      // send operation would cause a SIGPIPE signal, and the default behavior
      // is to terminate the whole process. That's why we need to ignore SIGPIPE
      // signal when the app is initializing itself. Please see ten_app_create()
      // for more detail.
      ten_protocol_integrated_send_buf(self, output_buf);
    }
  }
}

static void ten_stream_on_data_sent(ten_stream_t *stream, int status,
                                    TEN_UNUSED void *user_data) {
  TEN_ASSERT(stream, "Should not happen.");

  ten_protocol_integrated_t *protocol =
      (ten_protocol_integrated_t *)stream->user_data;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_attach_to(&protocol->base) ==
                     TEN_PROTOCOL_ATTACH_TO_CONNECTION &&
                 ten_protocol_role_is_communication(&protocol->base),
             "Should not happen.");

  if (status) {
    TEN_LOGI("Failed to send data, close the protocol: %d", status);

    ten_protocol_close(&protocol->base);
  } else {
    // Trigger myself again to send out more messages if possible.
    ten_protocol_integrated_on_output(protocol);
  }
}

static void ten_stream_on_data_free(TEN_UNUSED ten_stream_t *stream,
                                    TEN_UNUSED int status, void *user_data) {
  TEN_FREE(user_data);
}

static void ten_protocol_integrated_set_stream(ten_protocol_integrated_t *self,
                                               ten_stream_t *stream) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");
  TEN_ASSERT(stream, "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(&self->base),
             "Should not happen.");

  self->role_facility.communication_stream = stream;

  stream->user_data = self;
  stream->on_message_read = ten_stream_on_data;
  stream->on_message_sent = ten_stream_on_data_sent;
  stream->on_message_free = ten_stream_on_data_free;

  ten_stream_set_on_closed(stream, ten_protocol_integrated_on_stream_closed,
                           self);
}

static void ten_app_thread_on_client_protocol_created(ten_env_t *ten_env,
                                                      ten_protocol_t *instance,
                                                      void *cb_data) {
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");

  ten_protocol_integrated_t *protocol = (ten_protocol_integrated_t *)instance;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");

  ten_stream_t *stream = cb_data;
  TEN_ASSERT(stream, "Should not happen.");

  ten_protocol_on_client_accepted_func_t on_client_accepted = stream->user_data;
  TEN_ASSERT(on_client_accepted, "Should not happen.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_protocol_t *listening_base_protocol = app->endpoint_protocol;
  TEN_ASSERT(listening_base_protocol &&
                 ten_protocol_check_integrity(listening_base_protocol, true),
             "Should not happen.");

  ten_protocol_t *new_communication_base_protocol = &protocol->base;
  TEN_ASSERT(
      new_communication_base_protocol &&
          ten_protocol_check_integrity(new_communication_base_protocol, true),
      "Should not happen.");

  // Attach the newly created protocol to app first.
  ten_protocol_attach_to_app(new_communication_base_protocol,
                             listening_base_protocol->attached_target.app);

  TEN_UNUSED ten_connection_t *connection = on_client_accepted(
      listening_base_protocol, new_communication_base_protocol);
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "Should not happen.");

  ten_protocol_integrated_set_stream(protocol, stream);

  TEN_LOGD("Start read from stream");

  TEN_UNUSED int rc = ten_stream_start_read(stream);
  TEN_ASSERT(!rc, "ten_stream_start_read() failed: %d", rc);
}

static void ten_transport_on_client_accepted(ten_transport_t *transport,
                                             ten_stream_t *stream,
                                             TEN_UNUSED int status) {
  TEN_ASSERT(transport && stream, "Should not happen.");

  ten_protocol_integrated_t *listening_protocol = transport->user_data;
  TEN_ASSERT(listening_protocol, "Should not happen.");

  // The `on_client_accepted_data` in transport stores the `on_client_accepted`
  // callback function set by the TEN runtime.
  ten_protocol_on_client_accepted_func_t on_client_accepted =
      transport->on_client_accepted_user_data;
  TEN_ASSERT(on_client_accepted, "Should not happen.");

  stream->user_data = on_client_accepted;

  ten_protocol_t *listening_base_protocol = &listening_protocol->base;
  TEN_ASSERT(listening_base_protocol &&
                 ten_protocol_check_integrity(listening_base_protocol, true),
             "Should not happen.");

  ten_app_t *app = listening_base_protocol->attached_target.app;
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // We can _not_ know whether the protocol role is
  // 'TEN_PROTOCOL_ROLE_IN_INTERNAL' or 'TEN_PROTOCOL_ROLE_IN_EXTERNAL' until
  // the message received from the protocol is processed. Refer to
  // 'ten_connection_on_msgs()' and
  // 'ten_connection_handle_command_from_external_client()'.
  bool rc = ten_addon_create_protocol(
      app->ten_env,
      ten_string_get_raw_str(&listening_base_protocol->addon_host->name),
      ten_string_get_raw_str(&listening_base_protocol->addon_host->name),
      TEN_PROTOCOL_ROLE_IN_DEFAULT, ten_app_thread_on_client_protocol_created,
      stream, &err);
  TEN_ASSERT(rc, "Failed to create protocol, err: %s", ten_error_message(&err));

  ten_error_deinit(&err);
}

static void ten_protocol_integrated_listen(
    ten_protocol_t *self_, const char *uri,
    ten_protocol_on_client_accepted_func_t on_client_accepted) {
  ten_protocol_integrated_t *self = (ten_protocol_integrated_t *)self_;
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");
  TEN_ASSERT(uri, "Should not happen.");
  // Only when the protocol attached to a app could start listening.
  TEN_ASSERT(ten_protocol_attach_to(&self->base) == TEN_PROTOCOL_ATTACH_TO_APP,
             "Should not happen.");

  ten_runloop_t *loop =
      ten_app_get_attached_runloop(self->base.attached_target.app);
  TEN_ASSERT(loop, "Should not happen.");

  ten_transport_t *transport = ten_transport_create(loop);
  TEN_ASSERT(transport, "Should not happen.");

  self->base.role = TEN_PROTOCOL_ROLE_LISTEN;
  self->role_facility.listening_transport = transport;

  transport->user_data = self;
  // When a client connects, it is first handled using
  // `ten_transport_on_client_accepted`, and only afterward is the
  // `on_client_accepted` defined from TEN runtime. This way, some tasks can be
  // performed within the protocol at the transport/stream layer first, before
  // switching to the TEN runtime's `on_client_accepted` callback.
  transport->on_client_accepted = ten_transport_on_client_accepted;
  transport->on_client_accepted_user_data = on_client_accepted;

  ten_transport_set_close_cb(transport,
                             ten_protocol_integrated_on_transport_closed, self);

  ten_string_t *transport_uri = ten_protocol_uri_to_transport_uri(self_, uri);

  TEN_LOGI("%s start listening.", ten_string_get_raw_str(transport_uri));

  TEN_UNUSED int rc = ten_transport_listen(transport, transport_uri);
  if (rc) {
    TEN_LOGE("Failed to create a listening endpoint (%s): %d",
             ten_string_get_raw_str(transport_uri), rc);

    // TODO(xilin): Handle the error.
  }

  ten_string_destroy(transport_uri);
}

static void ten_protocol_integrated_on_output_task(void *self_,
                                                   TEN_UNUSED void *arg) {
  ten_protocol_integrated_t *self = (ten_protocol_integrated_t *)self_;
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");

  if (self->base.state != TEN_PROTOCOL_STATE_CLOSING) {
    // Execute the actual task if the 'protocol' is still alive.
    ten_protocol_integrated_on_output(self);
  }

  // The task is completed, so delete a reference to the 'protocol' to reflect
  // this fact.
  ten_ref_dec_ref(&self->base.ref);
}

/**
 * @brief Extension threads might directly call this function to send out
 * messages. For example, if extension threads want to send out data/
 * video_frame/ audio_frame messages, because it's unnecessary to perform some
 * bookkeeping operations for command-type messages, extension threads would
 * _not_ transfer the messages to the engine thread first, extension threads
 * will call this function directly to send out those messages. Therefore, in
 * order to maintain thread safety, this function will use runloop task to add
 * an async task to the attached runloop of the protocol to send out those
 * messages in the correct thread.
 */
static void ten_protocol_integrated_on_output_async(
    ten_protocol_integrated_t *self, ten_list_t *msgs) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(msgs, "Invalid argument.");

  ten_protocol_t *protocol = &self->base;
  TEN_ASSERT(protocol && ten_protocol_check_integrity(protocol, true) &&
                 ten_protocol_role_is_communication(protocol),
             "Should not happen.");

  int rc = ten_mutex_lock(protocol->out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_list_concat(&protocol->out_msgs, msgs);

  rc = ten_mutex_unlock(protocol->out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  // Before posting a runloop task, we have to add a reference to the
  // 'protocol', so that it will not be destroyed _before_ the runloop task is
  // executed.
  ten_ref_inc_ref(&protocol->ref);

  ten_runloop_t *loop = ten_protocol_get_attached_runloop(protocol);
  TEN_ASSERT(loop, "Should not happen.");

  rc = ten_runloop_post_task_tail(loop, ten_protocol_integrated_on_output_task,
                                  self, NULL);
  if (rc) {
    TEN_LOGW("Failed to post task to protocol's runloop: %d", rc);
    TEN_ASSERT(0, "Should not happen.");
  }
}

static void ten_protocol_integrated_on_server_finally_connected(
    ten_protocol_integrated_connect_to_context_t *cb_data, bool success) {
  TEN_ASSERT(cb_data, "Should not happen.");
  TEN_ASSERT(cb_data->on_server_connected, "Should not happen.");

  ten_protocol_integrated_t *protocol = cb_data->protocol;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");

  cb_data->on_server_connected(&cb_data->protocol->base, success);
  cb_data->on_server_connected = NULL;

  ten_protocol_integrated_connect_to_context_destroy(cb_data);

  if (protocol->base.state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_protocol_integrated_on_close(protocol);
  }
}

/**
 * @brief Callback function invoked when a transport connection attempt after a
 * retry completes.
 *
 * This function is called by the transport layer when a retry connection
 * attempt either succeeds or fails. It handles the connection result by:
 * 1. On success: Setting up the stream for communication, notifying the caller,
 *    and cleaning up the retry timer.
 * 2. On failure: Enabling the retry timer for another attempt if retry attempts
 *    remain available.
 *
 * The function ensures proper resource management regardless of the connection
 * outcome. It also handles the case where the protocol is in the closing state
 * during the connection attempt.
 *
 * @param transport The transport instance that attempted the connection.
 * @param stream The stream created for the connection (valid only on success).
 * @param status Status code (0 or positive for success, negative for failure).
 */
static void ten_transport_on_server_connected_after_retry(
    ten_transport_t *transport, ten_stream_t *stream, int status) {
  ten_protocol_integrated_t *protocol = transport->user_data;
  // Since the transport is created with the runloop of the engine, it is
  // currently in the engine thread.
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(&protocol->base),
             "Should not happen.");

  ten_protocol_integrated_connect_to_context_t *connect_to_context =
      transport->on_server_connected_user_data;
  TEN_ASSERT(connect_to_context, "Should not happen.");
  TEN_ASSERT(connect_to_context->on_server_connected, "Should not happen.");

  // If the protocol is being closed, clean up resources and return early.
  if (protocol->base.state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_stream_close(stream);

    // There is no need to explicitly stop and close `retry_timer` here, because
    // the protocol's closing flow will explicitly close `retry_timer`.

    // The ownership of the 'connect_to_context' is transferred to the timer, so
    // the 'connect_to_context' will be freed when the timer is closed.
    return;
  }

  TEN_ASSERT(protocol->retry_timer, "Should not happen.");

  bool success = status >= 0;

  if (success) {
    // Connection succeeded - set up the stream for communication.
    ten_protocol_integrated_set_stream(protocol, stream);

    // Notify the caller of successful connection.
    connect_to_context->on_server_connected(&protocol->base, success);
    transport->on_server_connected_user_data = NULL;
    // Set 'on_server_connected' to NULL to indicate that this callback has
    // already been called and to prevent it from being called again.
    connect_to_context->on_server_connected = NULL;

    // Start reading from the stream.
    ten_stream_start_read(stream);

    TEN_LOGD("Connect to %s successfully after retry",
             ten_string_get_raw_str(&connect_to_context->server_uri));

    // Clean up the retry timer as it's no longer needed.
    ten_timer_stop_async(protocol->retry_timer);
    ten_timer_close_async(protocol->retry_timer);
  } else {
    // Connection failed - clean up the stream.
    ten_stream_close(stream);

    // Reset the timer to retry again or close the timer if retry attempts are
    // exhausted.
    ten_timer_enable(protocol->retry_timer);

    TEN_LOGD("Failed to connect to %s after retry",
             ten_string_get_raw_str(&connect_to_context->server_uri));
  }
}

/**
 * @brief Callback function invoked when the retry timer is triggered.
 *
 * This function is called when the retry timer fires, indicating it's time to
 * attempt another connection to the server. It creates a new transport and
 * attempts to connect to the server URI stored in the connection context.
 *
 * @param self The timer that triggered this callback.
 * @param on_trigger_data User data associated with the timer (the connection
 * context).
 */
static void ten_protocol_integrated_on_retry_timer_triggered(
    TEN_UNUSED ten_timer_t *self, void *on_trigger_data) {
  ten_protocol_integrated_connect_to_context_t *connect_to_context =
      on_trigger_data;
  TEN_ASSERT(connect_to_context, "Should not happen.");

  ten_protocol_integrated_t *protocol = connect_to_context->protocol;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");

  ten_runloop_t *loop = ten_protocol_get_attached_runloop(&protocol->base);
  TEN_ASSERT(loop, "Should not happen.");

  ten_transport_t *transport = ten_transport_create(loop);
  transport->user_data = protocol;
  transport->on_server_connected =
      ten_transport_on_server_connected_after_retry;
  transport->on_server_connected_user_data = connect_to_context;

  TEN_LOGD("Retry to connect to %s",
           ten_string_get_raw_str(&connect_to_context->server_uri));

  int rc = ten_transport_connect(transport, &connect_to_context->server_uri);
  if (rc) {
    // If the 'ten_transport_connect' directly returns error, it could be due to
    // invalid parameters or other errors which cannot be solved by retrying.

    TEN_LOGW(
        "Failed to connect to %s due to invalid parameters or other fatal "
        "errors.",
        ten_string_get_raw_str(&connect_to_context->server_uri));

    transport->on_server_connected_user_data = NULL;
    ten_transport_close(transport);

    connect_to_context->on_server_connected(&protocol->base, false);
    // Set 'on_server_connected' to NULL to indicate that this callback has
    // already been called and to prevent it from being called again.
    connect_to_context->on_server_connected = NULL;

    // Since this is a fatal error that can't be resolved by retrying,
    // we stop and close the retry timer to clean up resources.
    ten_timer_stop_async(protocol->retry_timer);
    ten_timer_close_async(protocol->retry_timer);
  }

#if defined(_DEBUG)
  // Add some random delays in debug mode to simulate network latency.
  ten_random_sleep_range_ms(0, 100);
#endif
}

static void ten_protocol_integrated_on_retry_timer_closed(ten_timer_t *timer,
                                                          void *user_data) {
  TEN_ASSERT(timer, "Should not happen.");

  ten_protocol_integrated_connect_to_context_t *connect_to_context = user_data;
  TEN_ASSERT(connect_to_context, "Should not happen.");

  ten_protocol_integrated_t *protocol = connect_to_context->protocol;
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");

  // The retry timer is closed, so we need to set it to NULL to prevent
  // stop/close it again.
  protocol->retry_timer = NULL;

  if (connect_to_context->on_server_connected) {
    TEN_LOGD(
        "Retry timer is closed, but the connection to %s is not established "
        "yet",
        ten_string_get_raw_str(&connect_to_context->server_uri));

    ten_protocol_integrated_on_server_finally_connected(connect_to_context,
                                                        false);
  } else {
    ten_protocol_integrated_connect_to_context_destroy(connect_to_context);
  }

  // Since `retry_timer` is attached to the runloop, stopping the runloop before
  // `retry_timer` is fully closed can lead to unexpected issues (for example,
  // in libuv, stopping the runloop while there are still active handles can
  // cause memory leaks). Therefore, before `retry_timer` is completely closed,
  // `has_uncompleted_async_task` needs to be set to `true` to block the
  // protocol's closing flow.
  protocol->base.has_uncompleted_async_task = false;

  if (protocol->base.state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_protocol_integrated_on_close(protocol);
  }
}

/**
 * @brief Callback function invoked when a transport successfully connects to a
 * server.
 *
 * This function is called by the transport layer when a connection attempt to a
 * server either succeeds or fails. It handles the connection result by either:
 * 1. On success: Setting up the stream for communication.
 * 2. On failure: Either notifying the caller immediately or setting up retry
 *    logic.
 *
 * @param transport The transport instance that attempted the connection.
 * @param stream The stream created for the connection (valid only on success).
 * @param status Status code (0 or positive for success, negative for failure).
 */
static void ten_transport_on_server_connected(ten_transport_t *transport,
                                              ten_stream_t *stream,
                                              int status) {
  ten_protocol_integrated_t *protocol = transport->user_data;

  // Since the transport is created with the runloop of the engine, it is
  // currently in the engine thread.
  TEN_ASSERT(protocol, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&protocol->base, true),
             "Should not happen.");
  TEN_ASSERT(ten_protocol_role_is_communication(&protocol->base),
             "Should not happen.");

  TEN_ASSERT(!protocol->retry_timer, "Should not happen.");

  ten_protocol_integrated_connect_to_context_t *cb_data =
      transport->on_server_connected_user_data;
  TEN_ASSERT(cb_data, "Should not happen.");
  TEN_ASSERT(cb_data->on_server_connected, "Should not happen.");

  // This connection has already ended, so `has_uncompleted_async_task` is set
  // to `false` to unblock the closing flow, allowing it to proceed as intended.
  protocol->base.has_uncompleted_async_task = false;

  // If protocol is already closing, clean up and report failure.
  if (protocol->base.state == TEN_PROTOCOL_STATE_CLOSING) {
    ten_stream_close(stream);
    ten_protocol_integrated_on_server_finally_connected(cb_data, false);
    return;
  }

  bool success = status >= 0;

  if (success) {
    // Connection successful - notify caller and set up the stream.
    TEN_LOGD("Connect to %s successfully",
             ten_string_get_raw_str(&cb_data->server_uri));

    ten_protocol_integrated_on_server_finally_connected(cb_data, true);
    ten_protocol_integrated_set_stream(protocol, stream);
    ten_stream_start_read(stream);
  } else {
    TEN_LOGD("Failed to connect to %s",
             ten_string_get_raw_str(&cb_data->server_uri));

    // Connection failed - close the stream.
    ten_stream_close(stream);

    // Determine if we should retry the connection.
    bool need_retry =
        protocol->retry_config.enable && protocol->retry_config.max_retries > 0;

    if (!need_retry) {
      // No retry needed - report failure immediately.
      TEN_LOGD("No retry needed for %s",
               ten_string_get_raw_str(&cb_data->server_uri));

      ten_protocol_integrated_on_server_finally_connected(cb_data, false);
      return;
    }

    TEN_LOGD("Retry needed for %s",
             ten_string_get_raw_str(&cb_data->server_uri));

    // Set up retry timer.
    ten_runloop_t *loop = ten_protocol_get_attached_runloop(&protocol->base);
    TEN_ASSERT(loop, "Should not happen.");

    ten_timer_t *timer = ten_timer_create(
        loop, (uint64_t)protocol->retry_config.interval_ms * 1000,
        (int32_t)protocol->retry_config.max_retries, false);
    TEN_ASSERT(timer, "Should not happen.");

    protocol->retry_timer = timer;

    // Note that the ownership of the 'cb_data' is transferred to the timer.
    // The 'cb_data' will be freed when the timer is closed.
    ten_timer_set_on_triggered(
        timer, ten_protocol_integrated_on_retry_timer_triggered, cb_data);
    ten_timer_set_on_closed(
        timer, ten_protocol_integrated_on_retry_timer_closed, cb_data);

    // Since `retry_timer` is attached to the runloop, stopping the runloop
    // before `retry_timer` is fully closed can lead to unexpected issues (for
    // example, in libuv, stopping the runloop while there are still active
    // handles can cause memory leaks). Therefore, before `retry_timer` is
    // completely closed, `has_uncompleted_async_task` needs to be set to `true`
    // to block the protocol's closing flow.
    protocol->base.has_uncompleted_async_task = true;

    ten_timer_enable(timer);
  }
}

/**
 * @brief Initiates a connection to a remote server for an integrated protocol.
 *
 * This function attempts to establish a connection to a remote server using the
 * provided URI. It creates a transport layer connection and sets up the
 * necessary callbacks to handle connection success or failure. If the
 * connection fails immediately due to invalid parameters, it will report the
 * failure without attempting to retry.
 *
 * The function ensures that:
 * 1. It's called from the correct thread (engine thread).
 * 2. The protocol is properly attached to a connection.
 * 3. No retry timer is already active.
 *
 * @param self_ The protocol instance.
 * @param uri The URI of the remote server to connect to.
 * @param on_server_connected Callback function to be invoked when the
 * connection attempt completes (whether successful or not).
 */
static void ten_protocol_integrated_connect_to(
    ten_protocol_t *self_, const char *uri,
    ten_protocol_on_server_connected_func_t on_server_connected) {
  ten_protocol_integrated_t *self = (ten_protocol_integrated_t *)self_;
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");
  TEN_ASSERT(uri, "Should not happen.");
  TEN_ASSERT(
      ten_protocol_attach_to(&self->base) == TEN_PROTOCOL_ATTACH_TO_CONNECTION,
      "Should not happen.");

  // For integrated protocols, this function must be called in the engine
  // thread.
  TEN_ASSERT(
      ten_engine_check_integrity(
          self->base.attached_target.connection->attached_target.remote->engine,
          true),
      "Should not happen.");
  TEN_ASSERT(!self->retry_timer, "Should not happen.");

  ten_runloop_t *loop = ten_remote_get_attached_runloop(
      self->base.attached_target.connection->attached_target.remote);
  TEN_ASSERT(loop, "Should not happen.");

  ten_string_t *transport_uri = ten_protocol_uri_to_transport_uri(self_, uri);
  TEN_ASSERT(transport_uri, "Should not happen.");

  // Create a transport for the connection attempt.
  // Note: If connection fails, the transport needs to be closed.
  ten_transport_t *transport = ten_transport_create(loop);
  transport->user_data = self;
  transport->on_server_connected = ten_transport_on_server_connected;

  // Create a context to track this connection attempt.
  // This context will be freed once the 'on_server_connected' callback is
  // called.
  ten_protocol_integrated_connect_to_context_t *connect_to_server_context =
      ten_protocol_integrated_connect_to_context_create(
          self, ten_string_get_raw_str(transport_uri), on_server_connected,
          NULL);
  transport->on_server_connected_user_data = connect_to_server_context;

  TEN_LOGD("Connect to %s", ten_string_get_raw_str(transport_uri));

  // Attempt to connect to the remote server.
  int rc = ten_transport_connect(transport, transport_uri);
  if (rc) {
    TEN_LOGW("Failed to connect to %s", uri);
    // If ten_transport_connect returns an error immediately, it's likely due to
    // invalid parameters or other fatal errors that can't be solved by
    // retrying. In this case, we report the failure and clean up resources
    // without attempting to retry.
    ten_protocol_integrated_on_server_finally_connected(
        connect_to_server_context, false);

    ten_transport_close(transport);
  } else {
    // The connect flow has been successfully started. Regardless of whether it
    // succeeds or fails, the connect callback will be invoked. Therefore,
    // `has_uncompleted_async_task` needs to be used to block the protocol's
    // closing flow to prevent the protocol from being destroyed before the
    // connect callback is called.
    self->base.has_uncompleted_async_task = true;
  }

#if defined(_DEBUG)
  // Add some random delays in debug mode to simulate network latency.
  ten_random_sleep_range_ms(0, 100);
#endif

  ten_string_destroy(transport_uri);
}

static void ten_protocol_integrated_on_stream_cleaned(
    ten_protocol_integrated_t *self) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "We are in the app thread now.");

  // Integrated protocol has been cleaned, call on_cleaned_for_internal callback
  // now.
  self->base.on_cleaned_for_internal(&self->base);
}

static void ten_protocol_integrated_clean(ten_protocol_integrated_t *self) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");

  // Integrated protocol needs to _clean_ the containing stream. Because the
  // containing stream will be recreated in the engine thread again.

  ten_stream_set_on_closed(self->role_facility.communication_stream,
                           ten_protocol_integrated_on_stream_cleaned, self);
  ten_stream_close(self->role_facility.communication_stream);
}

static void ten_stream_migrated(ten_stream_t *stream, void **user_data) {
  ten_engine_t *engine = user_data[0];
  TEN_ASSERT(engine && ten_engine_check_integrity(engine, true),
             "The 'stream' has already been migrated to the engine thread. "
             "Therefore, this function is called in the engine thread.");

  ten_connection_t *connection = user_data[1];
  // The connection is created in the app thread, and _before_ the cleaning is
  // completed, the connection should still belongs to the app thread, and only
  // until the cleaning is completed, we can transfer the connection to the
  // target thread (e.g.: the engine thread or the client thread). Because this
  // function is called in the engine thread, so we can not perform thread
  // checking here, and need to be careful to consider thread safety when access
  // the connection instance before the cleaning is completed.
  TEN_ASSERT(connection &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_connection_check_integrity(connection, false),
             "Invalid argument.");

  ten_protocol_integrated_t *protocol =
      (ten_protocol_integrated_t *)connection->protocol;
  TEN_ASSERT(
      protocol &&
          // TEN_NOLINTNEXTLINE(thread-check)
          ten_protocol_check_integrity(&protocol->base, false),
      "The reason is the same as the above comments about 'connection'.");

  ten_shared_ptr_t *cmd = user_data[2];
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");

  TEN_FREE(user_data);

  if (stream) {
    // The 'connection' belongs to the app thread, so we can not call
    // ten_connection_clean() here directly, we have to switch to the app thread
    // to do it.
    ten_app_clean_connection_async(engine->app, connection);

    ten_engine_on_connection_cleaned(engine, connection, cmd);
    ten_shared_ptr_destroy(cmd);

    // The clean up of the connection is done, it's time to create new stream
    // and async which are bound to the engine event loop to the connection.
    ten_protocol_integrated_set_stream(protocol, stream);

    // Re-start to read from this stream when we have set it up correctly with
    // the correct newly created eventloop.
    ten_stream_start_read(stream);
  } else {
    TEN_ASSERT(0, "Failed to migrate protocol.");
  }
}

/**
 * @brief Migrate 'protocol' from the app thread to the engine thread.
 * 'integrated' protocol needs this because the containing 'stream' needs to be
 * transferred from the app thread to the engine thread.
 */
static void ten_protocol_integrated_migrate(ten_protocol_integrated_t *self,
                                            ten_engine_t *engine,
                                            ten_connection_t *connection,
                                            ten_shared_ptr_t *cmd) {
  TEN_ASSERT(self && ten_protocol_check_integrity(&self->base, true),
             "Should not happen.");
  TEN_ASSERT(engine &&
                 // TEN_NOLINTNEXTLINE(thread-check)
                 ten_engine_check_integrity(engine, false),
             "The function is called in the app thread, and will migrate the "
             "protocol to the engine thread.");
  TEN_ASSERT(engine->app && ten_app_check_integrity(engine->app, true),
             "The function is called in the app thread, and will migrate the "
             "protocol to the engine thread.");
  TEN_ASSERT(connection && ten_connection_check_integrity(connection, true),
             "'connection' belongs to app thread now.");
  TEN_ASSERT(cmd && ten_cmd_base_check_integrity(cmd), "Should not happen.");

  ten_stream_t *stream = self->role_facility.communication_stream;
  TEN_ASSERT(stream && ten_stream_check_integrity(stream),
             "Should not happen.");

  // Stop reading from the stream _before_ migration.
  ten_stream_stop_read(stream);

  void **user_data = (void **)TEN_MALLOC(3 * sizeof(void *));
  TEN_ASSERT(user_data, "Failed to allocate memory.");

  user_data[0] = engine;
  user_data[1] = connection;
  user_data[2] = ten_shared_ptr_clone(cmd);

  ten_stream_migrate(stream, ten_runloop_current(), engine->loop, user_data,
                     ten_stream_migrated);
}

static void ten_protocol_integrated_on_base_protocol_cleaned(
    ten_protocol_t *self, TEN_UNUSED bool is_migration_state_reset) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_protocol_check_integrity(self, true), "Should not happen.");

  // The integrated protocol determines the closing event based on the message
  // size read from the stream, and the stream will be frozen during the
  // migration. So there is no closing event after the migration is completed
  // here.

  ten_list_t msgs = TEN_LIST_INIT_VAL;
  TEN_DO_WITH_MUTEX_LOCK(self->in_lock,
                         { ten_list_swap(&msgs, &self->in_msgs); });

  if (ten_list_is_empty(&msgs)) {
    return;
  }

  ten_protocol_on_inputs_based_on_migration_state(self, &msgs);
  ten_list_clear(&msgs);
}

void ten_protocol_integrated_init(
    ten_protocol_integrated_t *self, const char *name,
    ten_protocol_integrated_on_input_func_t on_input,
    ten_protocol_integrated_on_output_func_t on_output) {
  TEN_ASSERT(self && name, "Should not happen.");

  ten_protocol_init(
      &self->base, name,
      (ten_protocol_close_func_t)ten_protocol_integrated_close,
      (ten_protocol_on_output_func_t)ten_protocol_integrated_on_output_async,
      ten_protocol_integrated_listen, ten_protocol_integrated_connect_to,
      (ten_protocol_migrate_func_t)ten_protocol_integrated_migrate,
      (ten_protocol_clean_func_t)ten_protocol_integrated_clean);

  self->base.role = TEN_PROTOCOL_ROLE_INVALID;
  self->base.on_cleaned_for_external =
      ten_protocol_integrated_on_base_protocol_cleaned;

  self->role_facility.communication_stream = NULL;
  self->role_facility.listening_transport = NULL;

  self->on_input = on_input;
  self->on_output = on_output;
  ten_protocol_integrated_retry_config_init(&self->retry_config);
  self->retry_timer = NULL;
}

ten_protocol_integrated_connect_to_context_t *
ten_protocol_integrated_connect_to_context_create(
    ten_protocol_integrated_t *self, const char *server_uri,
    ten_protocol_on_server_connected_func_t on_server_connected,
    void *user_data) {
  TEN_ASSERT(server_uri, "Invalid argument.");
  TEN_ASSERT(on_server_connected, "Invalid argument.");

  ten_protocol_integrated_connect_to_context_t *context =
      (ten_protocol_integrated_connect_to_context_t *)TEN_MALLOC(
          sizeof(ten_protocol_integrated_connect_to_context_t));
  TEN_ASSERT(context, "Failed to allocate memory.");

  ten_string_init_from_c_str_with_size(&context->server_uri, server_uri,
                                       strlen(server_uri));
  context->on_server_connected = on_server_connected;
  context->on_server_connected_user_data = user_data;
  context->protocol = self;

  return context;
}

void ten_protocol_integrated_connect_to_context_destroy(
    ten_protocol_integrated_connect_to_context_t *context) {
  TEN_ASSERT(context, "Invalid argument.");
  // Ensure the callback has been called and reset to NULL.
  TEN_ASSERT(!context->on_server_connected, "Invalid argument.");

  ten_string_deinit(&context->server_uri);
  TEN_FREE(context);
}
