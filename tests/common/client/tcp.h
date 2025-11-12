//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "ten_utils/io/socket.h"
#include "ten_utils/lib/smart_ptr.h"

#define TCP_CLIENT_CONNECT_RETRY_TIMES 200

typedef struct ten_test_tcp_client_t ten_test_tcp_client_t;

typedef ten_buf_t (*ten_test_tcp_client_msgs_to_buf_func_t)(ten_list_t *msgs);
typedef void (*ten_test_tcp_client_buf_to_msgs_func_t)(
    ten_test_tcp_client_t *client, void *data, size_t data_size,
    ten_list_t *msgs);

typedef struct ten_test_tcp_client_t {
  ten_string_t app_id;
  ten_socket_t *socket;

  ten_test_tcp_client_msgs_to_buf_func_t msgs_to_buf;
  ten_test_tcp_client_buf_to_msgs_func_t buf_to_msgs;
} ten_test_tcp_client_t;

TEN_RUNTIME_API bool ten_test_tcp_client_init(
    ten_test_tcp_client_t *self, const char *app_id,
    ten_test_tcp_client_msgs_to_buf_func_t msgs_to_buf,
    ten_test_tcp_client_buf_to_msgs_func_t buf_to_msgs);

TEN_RUNTIME_API void ten_test_tcp_client_deinit(ten_test_tcp_client_t *self);

TEN_RUNTIME_API ten_test_tcp_client_t *ten_test_tcp_client_create(
    const char *app_id, ten_test_tcp_client_msgs_to_buf_func_t msgs_to_buf,
    ten_test_tcp_client_buf_to_msgs_func_t buf_to_msgs);

TEN_RUNTIME_API void ten_test_tcp_client_destroy(ten_test_tcp_client_t *self);

TEN_RUNTIME_API bool ten_test_tcp_client_send_msg(ten_test_tcp_client_t *self,
                                                  ten_shared_ptr_t *msg);

TEN_RUNTIME_API ten_shared_ptr_t *ten_test_tcp_client_recv_msg(
    ten_test_tcp_client_t *self);

TEN_RUNTIME_API void ten_test_tcp_client_recv_msgs_batch(
    ten_test_tcp_client_t *self, ten_list_t *msgs);

TEN_RUNTIME_API ten_shared_ptr_t *ten_test_tcp_client_send_and_recv_msg(
    ten_test_tcp_client_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_API bool ten_test_tcp_client_send_data(
    ten_test_tcp_client_t *self, const char *graph_id,
    const char *extension_group_name, const char *extension_name, void *data,
    size_t size);

TEN_RUNTIME_API void ten_test_tcp_client_get_info(ten_test_tcp_client_t *self,
                                                  ten_string_t *ip,
                                                  uint16_t *port);

TEN_RUNTIME_API bool ten_test_tcp_client_close_app(ten_test_tcp_client_t *self);
