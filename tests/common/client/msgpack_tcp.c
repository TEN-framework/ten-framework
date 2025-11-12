//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "tests/common/client/msgpack_tcp.h"

#include <string.h>

#include "core_protocols/msgpack/common/parser.h"
#include "core_protocols/msgpack/msg/msg.h"
#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "tests/common/client/tcp.h"

static ten_buf_t ten_test_msgpack_tcp_client_msgs_to_buf(ten_list_t *msgs) {
  TEN_ASSERT(msgs && ten_list_check_integrity(msgs), "Invalid argument.");
  return ten_msgpack_serialize_msgs(msgs, NULL);
}

static void ten_test_msgpack_tcp_client_buf_to_msgs(
    ten_test_tcp_client_t *client, void *data, size_t data_size,
    ten_list_t *msgs) {
  TEN_ASSERT(client, "Invalid argument.");

  ten_test_msgpack_tcp_client_t *msgpack_client =
      (ten_test_msgpack_tcp_client_t *)client;
  ten_msgpack_deserialize_msgs(
      &msgpack_client->parser,
      TEN_BUF_STATIC_INIT_WITH_DATA_UNOWNED(data, data_size), msgs);
}

ten_test_msgpack_tcp_client_t *ten_test_msgpack_tcp_client_create(
    const char *app_id) {
  TEN_ASSERT(app_id, "Invalid argument.");

  ten_test_msgpack_tcp_client_t *client =
      (ten_test_msgpack_tcp_client_t *)TEN_MALLOC(
          sizeof(ten_test_msgpack_tcp_client_t));
  TEN_ASSERT(client, "Failed to allocate memory.");

  if (ten_test_tcp_client_init(&client->base, app_id,
                               ten_test_msgpack_tcp_client_msgs_to_buf,
                               ten_test_msgpack_tcp_client_buf_to_msgs)) {
    ten_msgpack_parser_init(&client->parser);
  } else {
    TEN_FREE(client);
    client = NULL;
  }

  return client;
}

void ten_test_msgpack_tcp_client_destroy(ten_test_msgpack_tcp_client_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_test_tcp_client_deinit(&self->base);
  ten_msgpack_parser_deinit(&self->parser);

  TEN_FREE(self);
}

bool ten_test_msgpack_tcp_client_send_msg(ten_test_msgpack_tcp_client_t *self,
                                          ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  return ten_test_tcp_client_send_msg(&self->base, msg);
}

ten_shared_ptr_t *ten_test_msgpack_tcp_client_recv_msg(
    ten_test_msgpack_tcp_client_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  return ten_test_tcp_client_recv_msg(&self->base);
}

void ten_test_msgpack_tcp_client_recv_msgs_batch(
    ten_test_msgpack_tcp_client_t *self, ten_list_t *msgs) {
  TEN_ASSERT(self && msgs, "Invalid argument.");

  return ten_test_tcp_client_recv_msgs_batch(&self->base, msgs);
}

ten_shared_ptr_t *ten_test_msgpack_tcp_client_send_and_recv_msg(
    ten_test_msgpack_tcp_client_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  return ten_test_tcp_client_send_and_recv_msg(&self->base, msg);
}

bool ten_test_msgpack_tcp_client_send_data(ten_test_msgpack_tcp_client_t *self,
                                           const char *graph_id,
                                           const char *extension_group_name,
                                           const char *extension_name,
                                           void *data, size_t size) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(graph_id && extension_group_name && extension_name && data,
             "Invalid argument.");

  return ten_test_tcp_client_send_data(
      &self->base, graph_id, extension_group_name, extension_name, data, size);
}

bool ten_test_msgpack_tcp_client_close_app(
    ten_test_msgpack_tcp_client_t *self) {
  return ten_test_tcp_client_close_app(&self->base);
}

void ten_test_msgpack_tcp_client_get_info(ten_test_msgpack_tcp_client_t *self,
                                          ten_string_t *ip, uint16_t *port) {
  ten_test_tcp_client_get_info(&self->base, ip, port);
}
