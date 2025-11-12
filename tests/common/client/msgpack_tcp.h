//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include "core_protocols/msgpack/common/parser.h"
#include "ten_utils/lib/smart_ptr.h"
#include "tests/common/client/tcp.h"

typedef struct ten_test_msgpack_tcp_client_t {
  ten_test_tcp_client_t base;
  ten_msgpack_parser_t parser;
} ten_test_msgpack_tcp_client_t;

TEN_RUNTIME_API ten_test_msgpack_tcp_client_t *
ten_test_msgpack_tcp_client_create(const char *app_id);

TEN_RUNTIME_API void ten_test_msgpack_tcp_client_destroy(
    ten_test_msgpack_tcp_client_t *self);

TEN_RUNTIME_API bool ten_test_msgpack_tcp_client_send_msg(
    ten_test_msgpack_tcp_client_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_API ten_shared_ptr_t *ten_test_msgpack_tcp_client_recv_msg(
    ten_test_msgpack_tcp_client_t *self);

TEN_RUNTIME_API void ten_test_msgpack_tcp_client_recv_msgs_batch(
    ten_test_msgpack_tcp_client_t *self, ten_list_t *msgs);

TEN_RUNTIME_PRIVATE_API ten_shared_ptr_t *
ten_test_msgpack_tcp_client_send_and_recv_msg(
    ten_test_msgpack_tcp_client_t *self, ten_shared_ptr_t *msg);

TEN_RUNTIME_PRIVATE_API bool ten_test_msgpack_tcp_client_send_data(
    ten_test_msgpack_tcp_client_t *self, const char *graph_id,
    const char *extension_group_name, const char *extension_name, void *data,
    size_t size);

TEN_RUNTIME_API void ten_test_msgpack_tcp_client_get_info(
    ten_test_msgpack_tcp_client_t *self, ten_string_t *ip, uint16_t *port);

TEN_RUNTIME_PRIVATE_API bool ten_test_msgpack_tcp_client_close_app(
    ten_test_msgpack_tcp_client_t *self);
