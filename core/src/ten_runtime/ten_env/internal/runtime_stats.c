//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/ten_env/runtime_stats.h"

#include <string.h>

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_utils/value/value_path.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/value/value_json.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)
#include "include_internal/ten_rust/ten_rust.h"
#endif

static const char runtime_stats_property[] = "ten:runtime_stats";

bool ten_env_is_runtime_stats_property(const char *path) {
  size_t prefix_len = sizeof(runtime_stats_property) - 1;
  return path && strncmp(path, runtime_stats_property, prefix_len) == 0 &&
         (path[prefix_len] == '\0' || path[prefix_len] == '.' ||
          path[prefix_len] == '[');
}

ten_value_t *ten_env_peek_runtime_stats(ten_env_t *self, const char *path,
                                        ten_error_t *err) {
  TEN_ASSERT(self && ten_env_check_integrity(self, true), "Invalid argument.");
  TEN_ASSERT(ten_env_is_runtime_stats_property(path), "Invalid argument.");

  ten_value_t *stats = NULL;
#if defined(TEN_ENABLE_TEN_RUST_APIS)
  // The service hub remains alive until all extension threads have stopped.
  ten_app_t *app = ten_env_get_belonging_app(self);
  if (app->service_hub.service_hub) {
    char *err_msg = NULL;
    const char *json = ten_service_hub_get_runtime_stats(
        app->service_hub.service_hub, &err_msg);
    if (!json) {
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Failed to collect runtime metrics: %s",
                      err_msg ? err_msg : "Unknown error.");
      }
      ten_rust_free_cstring(err_msg);
      return NULL;
    }

    stats = ten_value_from_json_str(json);
    ten_rust_free_cstring(json);
  } else
#endif
  {
    stats = ten_value_from_json_str("{\"metrics\":{\"resourceMetrics\":[]}}");
  }

  if (!stats) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_JSON,
                    "Failed to parse runtime statistics.");
    }
    return NULL;
  }

  if (self->runtime_stats) {
    ten_value_destroy(self->runtime_stats);
  }
  self->runtime_stats = stats;

  const char *sub_path = path + sizeof(runtime_stats_property) - 1;
  if (*sub_path == '\0') {
    return stats;
  }
  if (*sub_path == '.') {
    ++sub_path;
  }
  return ten_value_peek_from_path(stats, sub_path, err);
}
