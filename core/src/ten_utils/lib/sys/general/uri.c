//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/lib/uri.h"

#include <stdlib.h>

#include "include_internal/ten_utils/lib/safe_cast.h"

ten_string_t *ten_uri_get_protocol(const char *uri) {
  if (!uri) {
    return NULL;
  }

  size_t protocol_length = strcspn(uri, "://");
  if (protocol_length == 0) {
    return NULL;
  }

  int int_protocol_length = 0;
  bool rc = safe_cast_size_t_to_int(protocol_length, &int_protocol_length);
  TEN_ASSERT(rc, "Protocol length overflow detected.");

  return ten_string_create_formatted("%.*s", int_protocol_length, uri);
}

bool ten_uri_is_protocol_equal(const char *uri, const char *protocol) {
  if (!uri || !protocol) {
    return false;
  }

  size_t protocol_length = strcspn(uri, "://");
  if (protocol_length == 0) {
    return false;
  }

  return !strncmp(uri, protocol, protocol_length);
}

ten_string_t *ten_uri_get_host(const char *uri) {
  if (!uri) {
    return NULL;
  }

  size_t host_length = strcspn(uri, "://");
  if (host_length == 0) {
    return NULL;
  }

  const char *host = uri + host_length + 3;
  size_t host_length_with_port = strcspn(host, ":");
  if (host_length_with_port == 0) {
    return ten_string_create_formatted(host);
  }

  int int_host_length_with_port = 0;
  bool rc = safe_cast_size_t_to_int(host_length_with_port,
                                    &int_host_length_with_port);
  TEN_ASSERT(rc, "Host length with port overflow detected.");

  return ten_string_create_formatted("%.*s", int_host_length_with_port, host);
}

uint16_t ten_uri_get_port(const char *uri) {
  if (!uri) {
    return 0;
  }

  size_t host_length = strcspn(uri, "://");
  if (host_length == 0) {
    return 0;
  }

  const char *host = uri + host_length + 3;
  size_t host_length_with_port = strcspn(host, ":");
  if (host_length_with_port == 0) {
    return 0;
  }

  const char *port = host + host_length_with_port + 1;
  return atoi(port);
}
