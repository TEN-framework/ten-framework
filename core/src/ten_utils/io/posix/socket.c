//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ten_utils/lib/alloc.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/mark.h"

ten_socket_t *ten_socket_create(TEN_SOCKET_FAMILY family, TEN_SOCKET_TYPE type,
                                TEN_SOCKET_PROTOCOL protocol) {
  int32_t native_type = SOCK_STREAM;
  switch (type) {
  case TEN_SOCKET_TYPE_STREAM:
    native_type = SOCK_STREAM;
    break;

  case TEN_SOCKET_TYPE_DATAGRAM:
    native_type = SOCK_DGRAM;
    break;

  default:
    TEN_LOGE("Unknown socket type: %d", type);
    return NULL;
  }

  int fd = socket(family, native_type, protocol);
  if (fd < 0) {
    TEN_LOGE("Failed to create socket, fd: %d, errno: %d", fd, errno);
    return NULL;
  }

  ten_socket_t *ret = TEN_MALLOC(sizeof(ten_socket_t));
  TEN_ASSERT(ret, "Failed to allocate memory.");

  ret->fd = fd;
  ret->family = family;
  ret->protocol = protocol;
  ret->type = type;

  return ret;
}

void ten_socket_destroy(ten_socket_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  TEN_UNUSED int rc = close(self->fd);
  TEN_ASSERT(!rc, "Failed to close socket: %d(errno: %d)", rc, errno);

  TEN_FREE(self);
}
