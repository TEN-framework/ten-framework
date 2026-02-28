//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <string>

#include "ten_runtime/binding/cpp/detail/optional.h"

namespace ten {

struct loc_t {
  optional<std::string> app_uri;
  optional<std::string> graph_id;
  optional<std::string> extension_name;

  loc_t(const optional<std::string> &app_uri,
        const optional<std::string> &graph_id,
        const optional<std::string> &extension_name)
      : app_uri(app_uri), graph_id(graph_id), extension_name(extension_name) {}

  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  loc_t(const char *app_uri = nullptr, const char *graph_id = nullptr,
        const char *extension_name = nullptr)
      : app_uri((app_uri != nullptr) ? optional<std::string>(app_uri)
                                     : nullptr),
        graph_id((graph_id != nullptr) ? optional<std::string>(graph_id)
                                       : nullptr),
        extension_name((extension_name != nullptr)
                           ? optional<std::string>(extension_name)
                           : nullptr) {}
};

}  // namespace ten
