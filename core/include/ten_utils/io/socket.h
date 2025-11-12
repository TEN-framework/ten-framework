//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stdint.h>
#if defined(_WIN32)
// clang-format off
#include <Windows.h>
#include <In6addr.h>
#include <WinSock2.h>
// clang-format on
#else
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "ten_utils/lib/string.h"

typedef enum TEN_SOCKET_FAMILY {
  TEN_SOCKET_FAMILY_INET = AF_INET,   // IPv4
  TEN_SOCKET_FAMILY_INET6 = AF_INET6  // IPv6
} TEN_SOCKET_FAMILY;

typedef enum TEN_SOCKET_TYPE {
  TEN_SOCKET_TYPE_STREAM = 1,    // TCP
  TEN_SOCKET_TYPE_DATAGRAM = 2,  // UDP
} TEN_SOCKET_TYPE;

typedef enum TEN_SOCKET_PROTOCOL {
  TEN_SOCKET_PROTOCOL_TCP = 6,
  TEN_SOCKET_PROTOCOL_UDP = 17,
} TEN_SOCKET_PROTOCOL;

typedef struct ten_addr_port_t {
  ten_string_t *addr;
  uint16_t port;
} ten_addr_port_t;

typedef struct ten_socket_addr_t {
  TEN_SOCKET_FAMILY family;
  union {
    struct in_addr sin_addr;
    struct in6_addr sin6_addr;
  } addr;
  uint16_t port;
} ten_socket_addr_t;

typedef struct ten_socket_t {
  TEN_SOCKET_FAMILY family;
  TEN_SOCKET_PROTOCOL protocol;
  TEN_SOCKET_TYPE type;
  int fd;
} ten_socket_t;

// Socket address
TEN_UTILS_API ten_socket_addr_t *ten_socket_addr_create(const char *address,
                                                        uint16_t port);

TEN_UTILS_API void ten_socket_addr_destroy(ten_socket_addr_t *self);

// Socket
TEN_UTILS_API ten_socket_t *ten_socket_create(TEN_SOCKET_FAMILY family,
                                              TEN_SOCKET_TYPE type,
                                              TEN_SOCKET_PROTOCOL protocol);

TEN_UTILS_API void ten_socket_destroy(ten_socket_t *self);

TEN_UTILS_API bool ten_socket_connect(ten_socket_t *socket,
                                      ten_socket_addr_t *address);

TEN_UTILS_API ssize_t ten_socket_send(const ten_socket_t *self, void *buf,
                                      size_t buf_size);

TEN_UTILS_API ssize_t ten_socket_recv(const ten_socket_t *socket, void *buf,
                                      size_t buf_size);

TEN_UTILS_API ten_addr_port_t
ten_socket_peer_addr_port(const ten_socket_t *self);

TEN_UTILS_API void ten_socket_get_info(ten_socket_t *self, ten_string_t *ip,
                                       uint16_t *port);
