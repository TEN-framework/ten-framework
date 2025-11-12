//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/container/hash_handle.h"
#include "ten_utils/container/hash_table.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"

#define TEN_REMOTE_SIGNATURE 0xB4540BD80996AA45U

typedef struct ten_connection_t ten_connection_t;
typedef struct ten_remote_t ten_remote_t;
typedef struct ten_engine_t ten_engine_t;

typedef void (*ten_remote_on_closed_func_t)(ten_remote_t *self,
                                            void *on_closed_data);

typedef bool (*ten_remote_on_msg_func_t)(ten_remote_t *self,
                                         ten_shared_ptr_t *msg,
                                         void *on_msg_data);

typedef void (*ten_remote_on_server_connected_func_t)(ten_remote_t *self,
                                                      ten_shared_ptr_t *cmd);

typedef void (*ten_remote_on_error_func_t)(ten_remote_t *self,
                                           ten_shared_ptr_t *cmd);

typedef enum TEN_REMOTE_STATE {
  TEN_REMOTE_STATE_INIT,
  TEN_REMOTE_STATE_CLOSING,
  TEN_REMOTE_STATE_CLOSED,
} TEN_REMOTE_STATE;

struct ten_remote_t {
  ten_signature_t signature;
  ten_sanitizer_thread_check_t thread_check;

  ten_hashhandle_t hh_in_remote_table;

  TEN_REMOTE_STATE state;

  ten_string_t uri;

  // The role of 'remote' is a bridge between a 'connection' and a 'engine'.
  //
  //     ... <==> engine <==> remote <==> connection <==> ...
  //                           ^^^^
  ten_connection_t *connection;
  ten_engine_t *engine;

  // @{
  // Member functions.
  ten_remote_on_closed_func_t on_closed;
  void *on_closed_data;

  ten_remote_on_msg_func_t on_msg;
  void *on_msg_data;

  ten_remote_on_server_connected_func_t on_server_connected;
  ten_shared_ptr_t *on_server_connected_cmd;

  ten_remote_on_error_func_t on_error;
  // @}
};

TEN_RUNTIME_PRIVATE_API bool ten_remote_check_integrity(ten_remote_t *self,
                                                        bool check_thread);

TEN_RUNTIME_PRIVATE_API void ten_remote_destroy(ten_remote_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_remote_on_input(ten_remote_t *self,
                                                 ten_shared_ptr_t *msg,
                                                 ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_remote_send_msg(ten_remote_t *self,
                                                 ten_shared_ptr_t *msg,
                                                 ten_error_t *err);

TEN_RUNTIME_PRIVATE_API bool ten_remote_is_uri_equal_to(ten_remote_t *self,
                                                        const char *uri);

TEN_RUNTIME_PRIVATE_API ten_runloop_t *ten_remote_get_attached_runloop(
    ten_remote_t *self);

TEN_RUNTIME_PRIVATE_API void ten_remote_on_connection_closed(
    ten_connection_t *connection, void *on_closed_data);

TEN_RUNTIME_PRIVATE_API ten_remote_t *ten_remote_create_for_engine(
    const char *uri, ten_engine_t *engine, ten_connection_t *connection);

TEN_RUNTIME_PRIVATE_API void ten_remote_close(ten_remote_t *self);

TEN_RUNTIME_PRIVATE_API void ten_remote_connect_to(
    ten_remote_t *self, ten_remote_on_server_connected_func_t connected,
    ten_shared_ptr_t *on_server_connected_cmd,
    ten_remote_on_error_func_t on_error);
