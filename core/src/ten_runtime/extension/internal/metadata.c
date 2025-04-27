//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/extension/metadata.h"

#include <stddef.h>
#include <stdint.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/extension/extension.h"
#include "include_internal/ten_runtime/extension/extension_info/extension_info.h"
#include "include_internal/ten_runtime/extension/path_timer.h"
#include "include_internal/ten_runtime/extension/ten_env/metadata.h"
#include "include_internal/ten_runtime/extension_thread/extension_thread.h"
#include "include_internal/ten_utils/lib/placeholder.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_merge.h"

static bool ten_extension_determine_ten_namespace_properties(
    ten_extension_t *self, ten_value_t *ten_namespace_properties) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Invalid argument.");
  TEN_ASSERT(
      ten_namespace_properties && ten_value_is_object(ten_namespace_properties),
      "Invalid argument.");

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_value_object_foreach(ten_namespace_properties, iter) {
    ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Should not happen.");

    if (ten_string_is_equal_c_str(&kv->key, TEN_STR_PATH_TIMEOUT)) {
      if (ten_value_is_object(kv->value)) {
        ten_value_t *in_path_timeout_value =
            ten_value_object_peek(kv->value, TEN_STR_IN_PATH);
        if (in_path_timeout_value) {
          int64_t timeout = ten_value_get_int64(in_path_timeout_value, &err);
          if (timeout > 0) {
            self->path_timeout_info.in_path_timeout = timeout;
          }
        }

        ten_value_t *out_path_timeout_value =
            ten_value_object_peek(kv->value, TEN_STR_OUT_PATH);
        if (out_path_timeout_value) {
          int64_t timeout = ten_value_get_int64(out_path_timeout_value, &err);
          if (timeout > 0) {
            self->path_timeout_info.out_path_timeout = timeout;
          }
        }
      } else {
        int64_t path_timeout = ten_value_get_int64(kv->value, &err);
        if (path_timeout > 0) {
          self->path_timeout_info.in_path_timeout = path_timeout;
          self->path_timeout_info.out_path_timeout = path_timeout;
        }
      }
    }

    if (ten_string_is_equal_c_str(&kv->key, TEN_STR_PATH_CHECK_INTERVAL)) {
      int64_t check_interval = ten_value_get_int64(kv->value, &err);
      if (check_interval > 0) {
        self->path_timeout_info.check_interval = check_interval;
      }
    }
  }

  ten_error_deinit(&err);

  return true;
}

// It is unreasonable for 'in_path' to be removed due to timeout before
// 'out_path' is removed for the same reason. To eliminate the chance of
// 'in_path' being removed from the path table prior to the removal of
// 'out_path', we ensure that the timeout value for 'in_path' is greater than
// the sum of the timeout value of 'out_path' and the time-out-checking
// interval.
//
// Given the following scenario:
//
// Client ───► ExtensionA ──cmdA──► ExtensionB ──cmdB──► ExtensionC
//                ▲                    │  ▲                  │
//                │                    │  │                  │
//                └──────respA─────────┘  └───────respB──────┘
//
// ExtensionB responds to cmdA with respA only after it receives respB from
// the ExtensionC. So we have to ensure that, for ExtensionB, the in_path is
// removed after the out_path is removed.
//
// Suppose the timeout value for 'in_path' is 'x'.
// Suppose the timeout value for 'out_path' is 'y'.
// Suppose the time-out-checking interval is 'z'.
//
// Suppose the 'in_path' is added to the path table at time t0.
// Suppose the 'out_path' is added to the path table at time t0 + m (m > 0).
//
// The 'in_path' will be removed at time (t0 + x, t0 + x + z).
// The 'out_path' will be removed at time (t0 + m + y, t0 + m + y + z).
//
// To ensure that the 'in_path' is removed __after__ the 'out_path' is
// removed, we have to ensure that the earliest time for the 'in_path' to be
// removed is greater than the latest time for the 'out_path' to be removed.
// That is, we have to ensure that:
//
// t0 + x > t0 + m + y + z (m > 0)
// ===>  x > m + y + z (m > 0)
//
// 'm' can be any positive integer, ranging from potentially miniscule values
// like 10ns to substantial figures like 100s. Returning to the example above,
// if ExtensionB send cmdB to ExtensionC immediately after it receives cmdA
// from ExtensionA, then 'm' will be very small. We can almost guarantee that
// 'm' is less than 1s. So we can safely assume that 'm' is 1s. That is, we
// have to ensure that:
//
// x > 1s + y + z
//
// However, if ExtensionB send cmdB to ExtensionC after a certain period of
// time after it receives cmdA from ExtensionA, then 'm' will be very large.
// In this case, developers should set the timeout value for 'in_path' by
// themselves.
static void ten_extension_adjust_in_path_timeout(ten_extension_t *self) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  ten_path_timeout_info *path_timeout_info = &self->path_timeout_info;
  TEN_ASSERT(path_timeout_info, "Should not happen.");

  uint64_t in_path_min_timeout = UINT64_MAX;
  uint64_t one_sec_us = (uint64_t)1 * 1000 * 1000;

  // Considering the case of integer overflow, calculate at least what the
  // value of 'in_path timeout' should be.
  if (path_timeout_info->out_path_timeout <
      UINT64_MAX - path_timeout_info->check_interval) {
    in_path_min_timeout =
        path_timeout_info->out_path_timeout + path_timeout_info->check_interval;
  }

  // An extension by default has one second to process its own operations.
  if (in_path_min_timeout < UINT64_MAX - one_sec_us) {
    in_path_min_timeout += one_sec_us;
  }

  // Update the in_path timeout to the calculated minimal value if the
  // original value is too small.
  if (path_timeout_info->in_path_timeout < in_path_min_timeout) {
    path_timeout_info->in_path_timeout = in_path_min_timeout;
  }
}

// Retrieve those property fields that are reserved for the TEN runtime under
// the 'ten' namespace.
static ten_value_t *ten_extension_get_ten_namespace_properties(
    ten_extension_t *self) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  return ten_value_object_peek(&self->property, TEN_STR_TEN);
}

static bool ten_extension_graph_property_resolve_placeholders(
    ten_extension_t *self, ten_value_t *curr_value, ten_error_t *err) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  if (!curr_value || !ten_value_is_valid(curr_value)) {
    return false;
  }

  switch (curr_value->type) {
  case TEN_TYPE_INT8:
  case TEN_TYPE_INT16:
  case TEN_TYPE_INT32:
  case TEN_TYPE_INT64:
  case TEN_TYPE_UINT8:
  case TEN_TYPE_UINT16:
  case TEN_TYPE_UINT32:
  case TEN_TYPE_UINT64:
  case TEN_TYPE_FLOAT32:
  case TEN_TYPE_FLOAT64:
    return true;

  case TEN_TYPE_STRING: {
    const char *str_value = ten_value_peek_raw_str(curr_value, err);
    if (ten_c_str_is_placeholder(str_value)) {
      ten_placeholder_t placeholder;
      ten_placeholder_init(&placeholder);

      if (!ten_placeholder_parse(&placeholder, str_value, err)) {
        return false;
      }

      if (!ten_placeholder_resolve(&placeholder, curr_value, err)) {
        return false;
      }

      ten_placeholder_deinit(&placeholder);
    }
    return true;
  }

  case TEN_TYPE_OBJECT: {
    ten_value_object_foreach(curr_value, iter) {
      ten_value_kv_t *kv = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(kv && ten_value_kv_check_integrity(kv), "Should not happen.");

      ten_value_t *kv_value = ten_value_kv_get_value(kv);
      if (!ten_extension_graph_property_resolve_placeholders(self, kv_value,
                                                             err)) {
        return false;
      }
    }
    return true;
  }

  case TEN_TYPE_ARRAY: {
    ten_value_array_foreach(curr_value, iter) {
      ten_value_t *array_value = ten_ptr_listnode_get(iter.node);
      TEN_ASSERT(array_value && ten_value_check_integrity(array_value),
                 "Should not happen.");

      if (!ten_extension_graph_property_resolve_placeholders(self, array_value,
                                                             err)) {
        return false;
      }
    }
    return true;
  }

  default:
    return true;
  }

  TEN_ASSERT(0, "Should not happen.");
}

bool ten_extension_resolve_properties_in_graph(ten_extension_t *self,
                                               ten_error_t *err) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  if (!self->extension_info) {
    return true;
  }

  ten_value_t *graph_property = self->extension_info->property;
  if (!graph_property) {
    return true;
  }

  if (!ten_value_is_valid(graph_property)) {
    return false;
  }

  TEN_ASSERT(ten_value_is_object(graph_property), "Should not happen.");

  return ten_extension_graph_property_resolve_placeholders(self, graph_property,
                                                           err);
}

void ten_extension_merge_properties_from_graph(ten_extension_t *self) {
  TEN_ASSERT(self && ten_extension_check_integrity(self, true),
             "Should not happen.");

  // Merge properties in graph into the extension's property store.
  if (self->extension_info && self->extension_info->property) {
    ten_value_object_merge_with_clone(&self->property,
                                      self->extension_info->property);
  }
}

// Determine the internal properties of the extension according to the
// 'ten' object in the extension's property store. like:
// {
//   "ten": {
//     "path_timeout": {
//       "in_path": 5000000,
//       "out_path": 1000000,
//     },
//     "path_check_interval": 1000000
//   }
// }
bool ten_extension_handle_ten_namespace_properties(
    ten_extension_t *self, ten_extension_context_t *extension_context) {
  TEN_ASSERT(
      self && ten_extension_check_integrity(self, true) && extension_context,
      "Should not happen.");

  // This function is safe to be called from the extension main threads,
  // because all the resources it accesses are the
  // 'extension_info_from_graph', and all the
  // 'extension_info_from_graph' are setup completely before the
  // extension main threads are started, that means all the
  // 'extension_info_from_graph' will not be modified when this
  // function is being called.
  TEN_ASSERT(ten_extension_thread_call_by_me(self->extension_thread),
             "Should not happen.");

  ten_value_t *ten_namespace_properties =
      ten_extension_get_ten_namespace_properties(self);
  if (ten_namespace_properties == NULL) {
    TEN_LOGI("[%s] `%s` section is not found in the property, skip.",
             ten_extension_get_name(self, true), TEN_STR_TEN);
    return true;
  }

  ten_extension_determine_ten_namespace_properties(self,
                                                   ten_namespace_properties);

  ten_extension_adjust_in_path_timeout(self);

  return true;
}
