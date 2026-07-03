//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// clang-format off
#include <winsock2.h>
// clang-format on
#include "ten_utils/io/network.h"

#include <Windows.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <ws2tcpip.h>

#include "ten_utils/macro/macros.h"
#include "ten_utils/macro/memory.h"

#define OUT_LEN_BLOCK (4 * 1024)

void ten_host_get(char *hostname_buffer,
                  TEN_UNUSED const size_t hostname_buffer_capacity,
                  char *ip_buffer, const size_t ip_buffer_capacity) {
  TEN_UNUSED int rc = -1;
  PIP_ADAPTER_ADDRESSES addresses = NULL;
  PIP_ADAPTER_ADDRESSES cur_addr = NULL;
  PIP_ADAPTER_UNICAST_ADDRESS cur_ip = NULL;
  LPSOCKADDR sock_addr = NULL;
  void *in_addr = NULL;
  ULONG out_len = OUT_LEN_BLOCK;
  DWORD result = 0;
  DWORD retries = 0;

  TEN_ASSERT(hostname_buffer && hostname_buffer_capacity == URI_MAX_LEN &&
                 ip_buffer && ip_buffer_capacity == IP_STR_MAX_LEN,
             "Invalid argument.");

  // To retrieve hostname
  rc = gethostname(hostname_buffer, hostname_buffer_capacity);
  TEN_ASSERT(rc != -1, "Failed to get hostname.");

  // TEN_LOGD("Hostname: %s", hostname_buffer);

  do {
    addresses = (PIP_ADAPTER_ADDRESSES)TEN_MALLOC(out_len);
    result = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &out_len);
    if (result == ERROR_BUFFER_OVERFLOW) {
      TEN_FREE(addresses);
      addresses = NULL;
    } else {
      break;
    }
    retries++;
  } while (result == ERROR_BUFFER_OVERFLOW && retries < 3);

  if (result != ERROR_SUCCESS || !addresses) {
    // TEN_LOGD("Error: can not get adapter addresses");
    return;
  }

  // FIXME(Ender): strange implementation because of buggy interface
  // Currently always fetch first enabled one

  cur_addr = addresses;
  for (cur_addr = addresses; cur_addr != NULL; cur_addr = cur_addr->Next) {
    if (cur_addr->OperStatus != IfOperStatusUp) {
      continue;
    }

    for (cur_ip = cur_addr->FirstUnicastAddress; cur_ip != NULL;
         cur_ip = cur_ip->Next) {
      in_addr = NULL;
      sock_addr = cur_ip->Address.lpSockaddr;
      switch (sock_addr->sa_family) {
      case AF_INET: {
        struct sockaddr_in *s4 = (struct sockaddr_in *)sock_addr;
        in_addr = &s4->sin_addr;
        break;
      }
      case AF_INET6: {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)sock_addr;
        in_addr = &s6->sin6_addr;
        break;
      }
      default:
        continue;
      }

      if (!in_addr) {
        continue;
      }

      inet_ntop(sock_addr->sa_family, in_addr, ip_buffer, ip_buffer_capacity);
      break;
    }

    if (in_addr) {
      break;
    }
  }

  if (addresses) {
    TEN_FREE(addresses);
  }
}

bool ten_get_ipv6_prefix(const char *ifid, ten_string_t *prefix) {
  return false;
}
