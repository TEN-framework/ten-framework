//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/lib/smart_ptr.h"

typedef struct ten_engine_t ten_engine_t;
typedef struct ten_remote_t ten_remote_t;
typedef struct ten_connection_t ten_connection_t;

typedef void (*ten_engine_on_remote_created_cb_t)(ten_engine_t *engine,
                                                  ten_remote_t *remote,
                                                  void *user_data);
typedef void (*ten_engine_on_connected_to_graph_remote_cb_t)(
    ten_engine_t *engine, bool success, void *user_data);

typedef struct ten_engine_on_protocol_created_ctx_t {
  ten_engine_on_remote_created_cb_t cb;
  void *user_data;
} ten_engine_on_protocol_created_ctx_t;

TEN_RUNTIME_PRIVATE_API void ten_engine_upgrade_weak_remote_to_normal_remote(
    ten_engine_t *self, ten_remote_t *remote);

TEN_RUNTIME_PRIVATE_API bool ten_engine_connect_to_graph_remote(
    ten_engine_t *self, const char *uri, ten_shared_ptr_t *cmd);

TEN_RUNTIME_PRIVATE_API void ten_engine_route_msg_to_remote(
    ten_engine_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API ten_remote_t *ten_engine_check_remote_is_existed(
    ten_engine_t *self, const char *uri);

TEN_RUNTIME_PRIVATE_API bool ten_engine_check_remote_is_duplicated(
    ten_engine_t *self, const char *uri);

TEN_RUNTIME_PRIVATE_API bool ten_engine_check_remote_is_weak(
    ten_engine_t *self, ten_remote_t *remote);

TEN_RUNTIME_PRIVATE_API ten_remote_t *ten_engine_find_weak_remote(
    ten_engine_t *self, const char *uri);

TEN_RUNTIME_PRIVATE_API void ten_engine_on_remote_closed(ten_remote_t *remote,
                                                         void *on_closed_data);

TEN_RUNTIME_PRIVATE_API bool ten_engine_receive_msg_from_remote(
    ten_remote_t *remote, ten_shared_ptr_t *msg, void *user_data);

TEN_RUNTIME_PRIVATE_API void ten_engine_link_orphan_connection_to_remote(
    ten_engine_t *self, ten_connection_t *connection, const char *uri);
