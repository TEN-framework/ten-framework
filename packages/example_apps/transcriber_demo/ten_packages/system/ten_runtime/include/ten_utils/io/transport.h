//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/mutex.h"

typedef struct ten_transportbackend_t ten_transportbackend_t;
typedef struct ten_string_t ten_string_t;
typedef struct ten_stream_t ten_stream_t;

typedef enum TEN_TRANSPORT_DROP_TYPE {
  /* Drop oldest data when transport channel is full, only available when
     transport type is shm or raw pointer */
  TEN_TRANSPORT_DROP_OLD,

  /* Drop current data when transport channel is full */
  TEN_TRANSPORT_DROP_NEW,

  /* Drop current data if no reader is waiting */
  TEN_TRANSPORT_DROP_IF_NO_READER,

  /* Drop current data by asking, only available when
     transport type is shm or raw pointer .
     Useful if user wan't to drop by bussiness logic (e.g. never drop I frame)
   */
  TEN_TRANSPORT_DROP_ASK
} TEN_TRANSPORT_DROP_TYPE;

typedef struct ten_transport_t ten_transport_t;

struct ten_transport_t {
  /**
   * Runloop instance attached to the current thread that handles event
   * processing.
   */
  ten_runloop_t *loop;

  /**
   * User-defined data that can be associated with this transport.
   */
  void *user_data;

  /**
   * Backend implementation that handles the actual transport operations.
   */
  ten_transportbackend_t *backend;

  /**
   * Atomic flag indicating whether the transport is in the process of closing.
   */
  ten_atomic_t close;

  /**
   * Mutex for thread-safe access to transport properties.
   */
  ten_mutex_t *lock;

  /**
   * Flag indicating whether to drop messages when the transport channel is
   * full. Non-zero value enables dropping, zero disables it.
   */
  int drop_when_full;

  /**
   * Specifies the strategy for dropping messages when the channel is full.
   * @see TEN_TRANSPORT_DROP_TYPE enum for available strategies.
   */
  TEN_TRANSPORT_DROP_TYPE drop_type;

  /**
   * Callback invoked when a client successfully connects to a remote server.
   *
   * @param transport The transport instance that initiated the connection.
   * @param stream The stream created for the connection.
   * @param status Status code (0 for success, error code otherwise).
   */
  void (*on_server_connected)(ten_transport_t *transport, ten_stream_t *stream,
                              int status);

  /**
   * User data passed to the `on_server_connected` callback.
   */
  void *on_server_connected_user_data;

  /**
   * Callback invoked when a server accepts a new client connection.
   *
   * @param transport The transport instance that accepted the connection.
   * @param stream The stream created for the new client.
   * @param status Status code (0 for success, error code otherwise).
   */
  void (*on_client_accepted)(ten_transport_t *transport, ten_stream_t *stream,
                             int status);

  /**
   * User data passed to the on_client_accepted callback.
   */
  void *on_client_accepted_user_data;

  /**
   * Callback invoked when the transport is fully closed.
   *
   * @param on_closed_data User data provided when setting this callback.
   */
  void (*on_closed)(void *on_closed_user_data);

  /**
   * User data passed to the on_closed callback.
   */
  void *on_closed_user_data;
};

// Public interface
TEN_UTILS_API ten_transport_t *ten_transport_create(ten_runloop_t *loop);

TEN_UTILS_API int ten_transport_close(ten_transport_t *self);

TEN_UTILS_API void ten_transport_set_close_cb(ten_transport_t *self,
                                              void *close_cb,
                                              void *close_cb_data);

TEN_UTILS_API int ten_transport_listen(ten_transport_t *self,
                                       const ten_string_t *my_uri);

TEN_UTILS_API int ten_transport_connect(ten_transport_t *self,
                                        ten_string_t *dest);
