//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "tests/common/client/tcp.h"

#include <stddef.h>

#include "include_internal/ten_runtime/msg/msg.h"
#include "ten_runtime/msg/cmd/close_app/cmd.h"
#include "ten_runtime/ten.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_str.h"
#include "ten_utils/io/network.h"
#include "ten_utils/io/socket.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/buf.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/macro/check.h"

static void ten_test_tcp_client_dump_socket_info(ten_test_tcp_client_t *self,
                                                 const char *fmt, ...) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_string_t ip;
  TEN_STRING_INIT(ip);
  uint16_t port = 0;
  ten_socket_get_info(self->socket, &ip, &port);

  ten_string_t new_fmt;
  TEN_STRING_INIT(new_fmt);

  const char *p = fmt;

  while (*p) {
    if ('^' != *p) {
      ten_string_append_formatted(&new_fmt, "%c", *p++);
      continue;
    }

    switch (*++p) {
    // The IP.
    case '1':
      ten_string_append_formatted(&new_fmt, "%s", ten_string_get_raw_str(&ip));
      p++;
      break;

    // The port.
    case '2':
      ten_string_append_formatted(&new_fmt, "%d", port);
      p++;
      break;

    default:
      ten_string_append_formatted(&new_fmt, "%c", *p++);
      break;
    }
  }

  va_list ap;
  va_start(ap, fmt);

  ten_string_t description;
  TEN_STRING_INIT(description);
  ten_string_append_from_va_list(&description, ten_string_get_raw_str(&new_fmt),
                                 ap);
  ten_string_deinit(&new_fmt);

  va_end(ap);

  TEN_LOGD("%s", ten_string_get_raw_str(&description));

  ten_string_deinit(&ip);
  ten_string_deinit(&description);
}

bool ten_test_tcp_client_init(
    ten_test_tcp_client_t *self, const char *app_id,
    ten_test_tcp_client_msgs_to_buf_func_t msgs_to_buf,
    ten_test_tcp_client_buf_to_msgs_func_t buf_to_msgs) {
  TEN_ASSERT(self && app_id, "Invalid argument.");

  ten_string_init_formatted(&self->app_id, "%s", app_id);
  self->msgs_to_buf = msgs_to_buf;
  self->buf_to_msgs = buf_to_msgs;
  self->socket = NULL;

  ten_list_t splitted_strs = TEN_LIST_INIT_VAL;
  ten_c_string_split(app_id, "/", &splitted_strs);
  TEN_ASSERT(ten_list_size(&splitted_strs) == 2, "Invalid argument.");

  char ip[256] = {0};
  int32_t port = 0;
  ten_list_foreach (&splitted_strs, iter) {
    if (iter.index == 1) {
      ten_string_t *splitted_str = ten_str_listnode_get(iter.node);
      TEN_ASSERT(splitted_str, "Invalid argument.");

      ten_host_split(ten_string_get_raw_str(splitted_str), ip, 256, &port);
      break;
    }
  }
  ten_list_clear(&splitted_strs);

  // According to Linux 'connect' manpage, it says:
  //
  // If connect() fails, consider the state of the socket as unspecified.
  // Portable applications should close the socket and create a new one for
  // reconnecting.
  //
  // In Linux, we can reuse the same socket, and calling connect() several
  // times. But in Mac, if we do this, after the first connect() fails (it
  // might be correct, probably the peer endpoint is not ready), the
  // afterwards connect() would still fail with errno 22 (invalid argument).
  // So we have to close the original socket, and create a new socket when
  // retrying.

  for (size_t i = 0; i < TCP_CLIENT_CONNECT_RETRY_TIMES; ++i) {
    self->socket =
        ten_socket_create(TEN_SOCKET_FAMILY_INET, TEN_SOCKET_TYPE_STREAM,
                          TEN_SOCKET_PROTOCOL_TCP);
    TEN_ASSERT(self->socket, "Failed to create socket.");

    ten_socket_addr_t *addr = ten_socket_addr_create(ip, port);
    if (ten_socket_connect(self->socket, addr)) {
      ten_socket_addr_destroy(addr);
      break;
    }
    ten_socket_addr_destroy(addr);
    ten_socket_destroy(self->socket);
    self->socket = NULL;

    // To prevent from busy re-trying.
    ten_random_sleep_range_ms(0, 1000);
  }

  if (!self->socket) {
    TEN_LOGW("Failed to connect to %s:%d after retry %d times.", ip, port,
             TCP_CLIENT_CONNECT_RETRY_TIMES);
    return false;
  }

  return true;
}

ten_test_tcp_client_t *ten_test_tcp_client_create(
    const char *app_id, ten_test_tcp_client_msgs_to_buf_func_t msgs_to_buf,
    ten_test_tcp_client_buf_to_msgs_func_t buf_to_msgs) {
  TEN_ASSERT(app_id, "Invalid argument.");

  ten_test_tcp_client_t *client =
      (ten_test_tcp_client_t *)TEN_MALLOC(sizeof(ten_test_tcp_client_t));
  TEN_ASSERT(client, "Failed to allocate memory.");
  if (!client) {
    return NULL;
  }

  if (!ten_test_tcp_client_init(client, app_id, msgs_to_buf, buf_to_msgs)) {
    TEN_FREE(client);
    client = NULL;
  }

  return client;
}

void ten_test_tcp_client_deinit(ten_test_tcp_client_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_test_tcp_client_dump_socket_info(self, "Close tcp client: ^1:^2");

  ten_string_deinit(&self->app_id);
  ten_socket_destroy(self->socket);
}

void ten_test_tcp_client_destroy(ten_test_tcp_client_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_test_tcp_client_deinit(self);
  TEN_FREE(self);
}

bool ten_test_tcp_client_send_msg(ten_test_tcp_client_t *self,
                                  ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  if (ten_msg_get_dest_cnt(msg) == 0) {
    ten_msg_clear_and_set_dest(msg, ten_string_get_raw_str(&self->app_id), NULL,
                               NULL, NULL, NULL);
  }

  ten_list_t msgs = TEN_LIST_INIT_VAL;
  ten_list_push_smart_ptr_back(&msgs, msg);
  ten_buf_t buf = self->msgs_to_buf(&msgs);

  size_t sent_size = 0;
  while (true) {
    ssize_t rc = ten_socket_send(self->socket, buf.data + sent_size,
                                 buf.content_size - sent_size);
    if (rc < 0) {
      ten_test_tcp_client_dump_socket_info(
          self, "ten_socket_send (^1:^2) failed: %ld", rc);
      return false;
    }

    sent_size += rc;
    if (sent_size == buf.content_size) {
      break;
    }
  }

  free(buf.data);

  return true;
}

void ten_test_tcp_client_recv_msgs_batch(ten_test_tcp_client_t *self,
                                         ten_list_t *msgs) {
  TEN_ASSERT(self && msgs, "Should not happen.");

  while (true) {
    char recv_buf[8192] = {0};
    ssize_t recv_size = ten_socket_recv(self->socket, recv_buf, 8192);
    if (recv_size > 0) {
      self->buf_to_msgs(self, recv_buf, recv_size, msgs);

      if (ten_list_size(msgs) > 0) {
        break;
      }
    } else {
      ten_test_tcp_client_dump_socket_info(
          self, "ten_socket_recv (^1:^2) failed: %ld", recv_size);

      break;
    }
  }
}

ten_shared_ptr_t *ten_test_tcp_client_recv_msg(ten_test_tcp_client_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_list_t msgs = TEN_LIST_INIT_VAL;
  ten_test_tcp_client_recv_msgs_batch(self, &msgs);

  TEN_ASSERT(ten_list_size(&msgs) <= 1, "Should not happen.");

  if (ten_list_size(&msgs) == 1) {
    ten_shared_ptr_t *received_msg =
        ten_smart_ptr_listnode_get(ten_list_front(&msgs));
    ten_shared_ptr_t *received_msg_ = ten_shared_ptr_clone(received_msg);

    ten_list_clear(&msgs);

    return received_msg_;
  }

  return NULL;
}

ten_shared_ptr_t *ten_test_tcp_client_send_and_recv_msg(
    ten_test_tcp_client_t *self, ten_shared_ptr_t *msg) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(msg, "Invalid argument.");
  TEN_ASSERT(ten_msg_check_integrity(msg), "Invalid argument.");

  if (ten_test_tcp_client_send_msg(self, msg)) {
    return ten_test_tcp_client_recv_msg(self);
  } else {
    return NULL;
  }
}

bool ten_test_tcp_client_send_data(ten_test_tcp_client_t *self,
                                   const char *graph_id,
                                   const char *extension_group_name,
                                   const char *extension_name, void *data,
                                   size_t size) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(graph_id && extension_group_name && extension_name && data,
             "Invalid argument.");

  ten_shared_ptr_t *msg = ten_data_create("test", NULL);

  ten_buf_t buf;
  ten_buf_init_with_copying_data(&buf, data, size);
  ten_data_set_buf_with_move(msg, &buf);

  ten_msg_clear_and_set_dest(msg, ten_string_get_raw_str(&self->app_id),
                             graph_id, extension_group_name, extension_name,
                             NULL);

  bool rc = ten_test_tcp_client_send_msg(self, msg);
  ten_shared_ptr_destroy(msg);

  return rc;
}

bool ten_test_tcp_client_close_app(ten_test_tcp_client_t *self) {
  ten_shared_ptr_t *close_app_cmd = ten_cmd_close_app_create();
  bool rc = ten_test_tcp_client_send_msg(self, close_app_cmd);
  ten_shared_ptr_destroy(close_app_cmd);

  return rc;
}

void ten_test_tcp_client_get_info(ten_test_tcp_client_t *self, ten_string_t *ip,
                                  uint16_t *port) {
  TEN_ASSERT(self, "Invalid argument.");
  ten_socket_get_info(self->socket, ip, port);
}
