//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/socket.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/lib/string.h"

#if defined(_WIN32)
#include <WS2tcpip.h>
#include <ws2ipdef.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "ten_utils/lib/alloc.h"
#include "ten_utils/macro/check.h"

ten_socket_addr_t *ten_socket_addr_create(const char *address, uint16_t port) {
  ten_socket_addr_t *self =
      (ten_socket_addr_t *)TEN_MALLOC(sizeof(ten_socket_addr_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  self->port = port;

  if (inet_pton(AF_INET, address, &self->addr.sin_addr) > 0) {
    self->family = TEN_SOCKET_FAMILY_INET;
    return self;
  } else if (inet_pton(AF_INET6, address, &self->addr.sin6_addr) > 0) {
    self->family = TEN_SOCKET_FAMILY_INET6;
    return self;
  } else {
    TEN_FREE(self);
    return NULL;
  }
}

void ten_socket_addr_destroy(ten_socket_addr_t *self) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_FREE(self);
}

static bool ten_socket_addr_to_native(const ten_socket_addr_t *addr, void *buf,
                                      size_t buf_size) {
  TEN_ASSERT(addr && buf && buf_size, "Invalid argument.");

  if (addr->family == TEN_SOCKET_FAMILY_INET) {
    if (buf_size < sizeof(struct sockaddr_in)) {
      // TEN_LOGE("Invalid buffer size for IPv4");
      return false;
    }

    struct sockaddr_in *sin = (struct sockaddr_in *)buf;
    memcpy(&sin->sin_addr, &addr->addr.sin_addr, sizeof(struct in_addr));
    sin->sin_family = AF_INET;
    sin->sin_port = htons(addr->port);
    memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
    return true;
  } else if (addr->family == TEN_SOCKET_FAMILY_INET6) {
    if (buf_size < sizeof(struct sockaddr_in6)) {
      // TEN_LOGE("Invalid buffer size for IPv6");
      return false;
    }

    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)buf;
    memcpy(&sin6->sin6_addr, &addr->addr.sin6_addr, sizeof(struct in6_addr));
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(addr->port);
    return true;
  }
  return false;
}

static size_t ten_socket_addr_get_native_size(const ten_socket_addr_t *addr) {
  TEN_ASSERT(addr, "Invalid argument.");

  if (addr->family == TEN_SOCKET_FAMILY_INET) {
    return sizeof(struct sockaddr_in);
  } else if (addr->family == TEN_SOCKET_FAMILY_INET6) {
    return sizeof(struct sockaddr_in6);
  } else {
    // TEN_LOGE("Unsupported socket family.");
    return 0;
  }
}

bool ten_socket_connect(ten_socket_t *self, ten_socket_addr_t *address) {
  TEN_ASSERT(self && address, "Invalid argument.");

  struct sockaddr_storage buffer;
  if (!ten_socket_addr_to_native(address, &buffer, sizeof(buffer))) {
    // TEN_LOGE("Failed to get native socket address.");
    return false;
  }

  int rc = connect(self->fd, (struct sockaddr *)&buffer,
                   ten_socket_addr_get_native_size(address));
  return rc ? false : true;
}

void ten_socket_get_info(ten_socket_t *self, ten_string_t *ip, uint16_t *port) {
  TEN_ASSERT(self, "Invalid argument.");

  struct sockaddr_in socket_info;
  socklen_t socket_info_size = sizeof(socket_info);
  getsockname(self->fd, (struct sockaddr *)&socket_info, &socket_info_size);

  char ip_buf[INET_ADDRSTRLEN + 1];
  ten_string_set_formatted(
      ip, "%s",
      inet_ntop(AF_INET, &socket_info.sin_addr, ip_buf, sizeof(ip_buf)));

  *port = ntohs(socket_info.sin_port);
}

ssize_t ten_socket_send(const ten_socket_t *self, void *buf, size_t buf_size) {
  TEN_ASSERT(self && buf && buf_size, "Invalid argument.");

  ssize_t rc = send(self->fd, buf, buf_size, 0);
  if (rc < 0) {
    TEN_LOGE("send() failed: %zd(errno: %d)", rc, errno);
    return -1;
  }

  return rc;
}

ssize_t ten_socket_recv(const ten_socket_t *socket, void *buf,
                        size_t buf_size) {
  TEN_ASSERT(socket && buf && buf_size, "Invalid argument.");

  ssize_t rc = recv(socket->fd, buf, buf_size, 0);
  if (rc < 0) {
    TEN_LOGE("recv() failed: %zd(errno: %d)", rc, errno);
    return -1;
  }
  return rc;
}

ten_addr_port_t ten_socket_peer_addr_port(const ten_socket_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  // Get client address and port.
  char ip_buf[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN
                                                 : INET_ADDRSTRLEN] = {0};
  struct sockaddr_storage sockaddr;
  int sockaddr_len = sizeof(sockaddr);
  getpeername(self->fd, (struct sockaddr *)&sockaddr,
              (socklen_t *)&sockaddr_len);
  switch (sockaddr.ss_family) {
  case AF_INET: {
    struct sockaddr_in *addr_in = (struct sockaddr_in *)&sockaddr;
    const char *ip =
        inet_ntop(AF_INET, &addr_in->sin_addr, ip_buf, INET_ADDRSTRLEN);

    return (ten_addr_port_t){ten_string_create_formatted(ip),
                             addr_in->sin_port};
  }

  case AF_INET6: {
    struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&sockaddr;
    const char *ip =
        inet_ntop(AF_INET6, &addr_in6->sin6_addr, ip_buf, INET6_ADDRSTRLEN);

    return (ten_addr_port_t){ten_string_create_formatted(ip),
                             addr_in6->sin6_port};
  }

  default:
    TEN_ASSERT(0, "Should handle more types: %d", sockaddr.ss_family);
    return (ten_addr_port_t){NULL, 0};
  }
}
