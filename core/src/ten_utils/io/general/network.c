//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_utils/io/network.h"

#include <stdlib.h>

#include "ten_utils/macro/check.h"

bool ten_host_split(const char *uri, char *name_buffer,
                    const size_t name_buffer_capacity, int32_t *port) {
  TEN_ASSERT(uri && name_buffer && port, "Invalid argument.");

  bool has_port = true;

  const char *colon = strchr(uri, ':');
  if (!colon) {
    colon = uri + strlen(uri);
    has_port = false;
  }

  const size_t address_length = colon - uri;
  if ((address_length + sizeof(char)) > name_buffer_capacity) {
    // Insufficient capacity.
    return false;
  }

  memcpy(name_buffer, uri, address_length);
  name_buffer[address_length] = '\0';

  if (has_port) {
    const char *port_str = colon + 1;
    const long port_number = strtol(port_str, NULL, 10);
    if (port_number > PORT_MAX_NUM) {
      // Incorrect port number.
      return false;
    }
    *port = (int32_t)port_number;
  } else {
    *port = 0;
  }

  return true;
}
