//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/network.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(TEN_ENABLE_OWN_IFADDR)
TEN_UTILS_API int getifaddrs(struct ifaddrs **ifap);
TEN_UTILS_API void freeifaddrs(struct ifaddrs *ifa);

void ten_host_get(char *hostname_buffer,
                  TEN_UNUSED const size_t hostname_buffer_capacity,
                  char *ip_buffer, const size_t ip_buffer_capacity) {
  TEN_ASSERT(hostname_buffer && hostname_buffer_capacity == URI_MAX_LEN &&
                 ip_buffer && ip_buffer_capacity == IP_STR_MAX_LEN,
             "Invalid argument.");

  // To retrieve hostname
  TEN_UNUSED int rc = gethostname(hostname_buffer, hostname_buffer_capacity);
  TEN_ASSERT(rc != -1, "Failed to get hostname.");

  // TEN_LOGD("Hostname: %s", hostname_buffer);

  // To retrieve host information, and get IP from it.
  struct ifaddrs *myaddrs = NULL;
  rc = getifaddrs(&myaddrs);
  TEN_ASSERT(rc == 0, "Failed to get host information.");

  for (struct ifaddrs *ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if ((ifa->ifa_addr == NULL) ||
        ((ifa->ifa_flags & (unsigned int)IFF_UP) == 0)) {
      continue;
    }

    void *in_addr = NULL;
    switch (ifa->ifa_addr->sa_family) {
    case AF_INET: {
      struct sockaddr_in *s4 = (struct sockaddr_in *)(ifa->ifa_addr);
      in_addr = &s4->sin_addr;
      break;
    }
    case AF_INET6: {
      struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)(ifa->ifa_addr);
      in_addr = &s6->sin6_addr;
      break;
    }
    default:
      continue;
    }

    TEN_UNUSED const char *result = inet_ntop(ifa->ifa_addr->sa_family, in_addr,
                                              ip_buffer, ip_buffer_capacity);
    TEN_ASSERT(result != NULL, "Failed to convert IP to string.");

    // TEN_LOGD("IP: %s", ip_buffer);
    break;
  }

  freeifaddrs(myaddrs);
}
#endif

bool ten_get_ipv6_prefix(const char *ifid, ten_string_t *prefix) {
  return false;
}
