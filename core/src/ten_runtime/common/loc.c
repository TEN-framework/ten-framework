//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/loc.h"

#include <stdlib.h>

#include "include_internal/ten_runtime/common/constant_str.h"
#include "ten_runtime/common/error_code.h"
#include "ten_runtime/common/loc.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_ptr.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_kv.h"
#include "ten_utils/value/value_object.h"

bool ten_loc_check_integrity(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_LOC_SIGNATURE) {
    return false;
  }

  return true;
}

ten_loc_t *ten_loc_create_empty(void) {
  ten_loc_t *self = (ten_loc_t *)TEN_MALLOC(sizeof(ten_loc_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_loc_init_empty(self);

  return self;
}

ten_loc_t *ten_loc_create(const char *app_uri, const char *graph_id,
                          const char *extension_name) {
  ten_loc_t *self = ten_loc_create_empty();

  ten_loc_set(self, app_uri, graph_id, extension_name);
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  return self;
}

ten_loc_t *ten_loc_create_from_value(ten_value_t *value) {
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  ten_loc_t *self = ten_loc_create_empty();

  ten_loc_set_from_value(self, value);
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  return self;
}

ten_loc_t *ten_loc_clone(ten_loc_t *src) {
  TEN_ASSERT(src, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(src), "Should not happen.");

  ten_loc_t *self = ten_loc_create(
      src->has_app_uri ? ten_string_get_raw_str(&src->app_uri) : NULL,
      src->has_graph_id ? ten_string_get_raw_str(&src->graph_id) : NULL,
      src->has_extension_name ? ten_string_get_raw_str(&src->extension_name)
                              : NULL);

  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  return self;
}

void ten_loc_copy(ten_loc_t *self, ten_loc_t *src) {
  TEN_ASSERT(self, "Invalid argument.");
  TEN_ASSERT(src, "Invalid argument.");
  TEN_ASSERT(ten_loc_check_integrity(src), "Invalid argument.");

  ten_loc_set_from_loc(self, src);
}

void ten_loc_destroy(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_loc_deinit(self);
  TEN_FREE(self);
}

void ten_loc_init_empty(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, TEN_LOC_SIGNATURE);

  self->has_app_uri = false;
  self->has_graph_id = false;
  self->has_extension_name = false;

  TEN_STRING_INIT(self->app_uri);
  TEN_STRING_INIT(self->graph_id);
  TEN_STRING_INIT(self->extension_name);
}

void ten_loc_init_from_loc(ten_loc_t *self, ten_loc_t *src) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(src, "Should not happen.");

  ten_signature_set(&self->signature, TEN_LOC_SIGNATURE);

  ten_loc_init(
      self, src->has_app_uri ? ten_string_get_raw_str(&src->app_uri) : NULL,
      src->has_graph_id ? ten_string_get_raw_str(&src->graph_id) : NULL,
      src->has_extension_name ? ten_string_get_raw_str(&src->extension_name)
                              : NULL);

  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
}

void ten_loc_set_from_loc(ten_loc_t *self, ten_loc_t *src) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(src, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_loc_set(
      self, src->has_app_uri ? ten_string_get_raw_str(&src->app_uri) : NULL,
      src->has_graph_id ? ten_string_get_raw_str(&src->graph_id) : NULL,
      src->has_extension_name ? ten_string_get_raw_str(&src->extension_name)
                              : NULL);
}

void ten_loc_deinit(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  ten_signature_set(&self->signature, 0);

  ten_string_deinit(&self->app_uri);
  ten_string_deinit(&self->graph_id);
  ten_string_deinit(&self->extension_name);
}

static void ten_loc_init_app_uri_with_unset_concept(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_init(&self->app_uri);
  self->has_app_uri = false;
}

static void ten_loc_init_graph_id_with_unset_concept(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_init(&self->graph_id);
  self->has_graph_id = false;
}

static void ten_loc_init_extension_name_with_unset_concept(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_init(&self->extension_name);
  self->has_extension_name = false;
}

void ten_loc_init(ten_loc_t *self, const char *app_uri, const char *graph_id,
                  const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");

  if (app_uri) {
    ten_loc_init_app_uri(self, app_uri);
  } else {
    ten_loc_init_app_uri_with_unset_concept(self);
  }

  if (graph_id) {
    ten_loc_init_graph_id(self, graph_id);
  } else {
    ten_loc_init_graph_id_with_unset_concept(self);
  }

  if (extension_name) {
    ten_loc_init_extension_name(self, extension_name);
  } else {
    ten_loc_init_extension_name_with_unset_concept(self);
  }

  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
}

void ten_loc_set(ten_loc_t *self, const char *app_uri, const char *graph_id,
                 const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");

  ten_loc_clear(self);

  if (app_uri) {
    ten_loc_set_app_uri(self, app_uri);
  }

  if (graph_id) {
    ten_loc_set_graph_id(self, graph_id);
  }

  if (extension_name) {
    ten_loc_set_extension_name(self, extension_name);
  }

  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
}

bool ten_loc_is_empty(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (!self->has_app_uri && !self->has_graph_id && !self->has_extension_name) {
    return true;
  }
  return false;
}

void ten_loc_clear(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  self->has_app_uri = false;
  self->has_graph_id = false;
  self->has_extension_name = false;

  ten_string_clear(&self->app_uri);
  ten_string_clear(&self->graph_id);
  ten_string_clear(&self->extension_name);
}

bool ten_loc_is_equal(ten_loc_t *self, ten_loc_t *other) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(other, "Should not happen.");

  return self->has_app_uri == other->has_app_uri &&
         self->has_graph_id == other->has_graph_id &&
         self->has_extension_name == other->has_extension_name &&
         (self->has_app_uri &&
          ten_string_is_equal(&self->app_uri, &other->app_uri)) &&
         (self->has_graph_id &&
          ten_string_is_equal(&self->graph_id, &other->graph_id)) &&
         (self->has_extension_name &&
          ten_string_is_equal(&self->extension_name, &other->extension_name));
}

/**
 * @brief Converts a location structure to a human-readable string
 * representation.
 *
 * This function formats the contents of a ten_loc_t structure into a string
 * that shows all components of the location (app URI, graph ID, extension
 * group, and extension name). Empty fields will appear as empty strings in the
 * output.
 *
 * @param self The location structure to convert to string.
 * @param result A pre-initialized string where the result will be stored.
 *               The previous contents of this string will be overwritten.
 *
 * @note The caller is responsible for ensuring that both parameters are valid
 *       and that result has been properly initialized.
 */
void ten_loc_to_string(ten_loc_t *self, ten_string_t *result) {
  TEN_ASSERT(self, "Invalid parameters or corrupted location structure.");
  TEN_ASSERT(ten_loc_check_integrity(self),
             "Invalid parameters or corrupted location structure.");
  TEN_ASSERT(result, "Invalid parameters or corrupted location structure.");

  ten_string_set_formatted(
      result, "app: %s, graph: %s, extension: %s",
      self->has_app_uri ? ten_string_get_raw_str(&self->app_uri)
                        : TEN_STR_LOC_EMPTY,
      self->has_graph_id ? ten_string_get_raw_str(&self->graph_id)
                         : TEN_STR_LOC_EMPTY,
      self->has_extension_name ? ten_string_get_raw_str(&self->extension_name)
                               : TEN_STR_LOC_EMPTY);
}

static bool ten_loc_set_value(ten_loc_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");
  TEN_ASSERT(ten_value_check_integrity(value), "Should not happen.");

  ten_list_t loc_fields = TEN_LIST_INIT_VAL;

  if (self->has_app_uri) {
    ten_list_push_ptr_back(
        &loc_fields,
        ten_value_kv_create(
            TEN_STR_APP,
            ten_value_create_string(ten_string_get_raw_str(&self->app_uri))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  if (self->has_graph_id) {
    ten_list_push_ptr_back(
        &loc_fields,
        ten_value_kv_create(
            TEN_STR_GRAPH,
            ten_value_create_string(ten_string_get_raw_str(&self->graph_id))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  if (self->has_extension_name) {
    ten_list_push_ptr_back(
        &loc_fields,
        ten_value_kv_create(TEN_STR_EXTENSION,
                            ten_value_create_string(
                                ten_string_get_raw_str(&self->extension_name))),
        (ten_ptr_listnode_destroy_func_t)ten_value_kv_destroy);
  }

  bool rc = ten_value_init_object_with_move(value, &loc_fields);

  ten_list_clear(&loc_fields);
  return rc;
}

ten_value_t *ten_loc_to_value(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_value_t *loc_value = ten_value_create_object_with_move(NULL);
  TEN_ASSERT(loc_value, "Should not happen.");

  if (ten_loc_set_value(self, loc_value)) {
    return loc_value;
  } else {
    ten_value_destroy(loc_value);
    return NULL;
  }
}

static void ten_loc_unset_app_uri(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_clear(&self->app_uri);
  self->has_app_uri = false;
}

static void ten_loc_unset_graph_id(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_clear(&self->graph_id);
  self->has_graph_id = false;
}

static void ten_loc_unset_extension_name(ten_loc_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");

  ten_string_clear(&self->extension_name);
  self->has_extension_name = false;
}

void ten_loc_set_from_value(ten_loc_t *self, ten_value_t *value) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(value, "Should not happen.");

  ten_value_t *app_value = ten_value_object_peek(value, TEN_STR_APP);
  ten_value_t *graph_value = ten_value_object_peek(value, TEN_STR_GRAPH);
  ten_value_t *extension_value =
      ten_value_object_peek(value, TEN_STR_EXTENSION);

  if (app_value) {
    TEN_ASSERT(ten_value_is_string(app_value), "Should not happen.");

    const char *app_str = ten_value_peek_raw_str(app_value, NULL);
    if (app_str && strlen(app_str) > 0) {
      ten_loc_set_app_uri(self, app_str);
    }
  } else {
    ten_loc_unset_app_uri(self);
  }

  if (graph_value) {
    TEN_ASSERT(ten_value_is_string(graph_value), "Should not happen.");

    const char *graph_str = ten_value_peek_raw_str(graph_value, NULL);
    if (graph_str && strlen(graph_str) > 0) {
      ten_loc_set_graph_id(self, graph_str);
    }
  } else {
    ten_loc_unset_graph_id(self);
  }

  if (extension_value) {
    TEN_ASSERT(ten_value_is_string(extension_value), "Should not happen.");

    const char *extension_name_str =
        ten_value_peek_raw_str(extension_value, NULL);
    if (extension_name_str && strlen(extension_name_str) > 0) {
      ten_loc_set_extension_name(self, extension_name_str);
    }
  } else {
    ten_loc_unset_extension_name(self);
  }
}

void ten_loc_init_app_uri_with_size(ten_loc_t *self, const char *app_uri,
                                    size_t app_uri_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(app_uri, "Should not happen.");

  ten_string_init_from_c_str_with_size(&self->app_uri, app_uri, app_uri_len);
  self->has_app_uri = true;
}

void ten_loc_init_app_uri(ten_loc_t *self, const char *app_uri) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(app_uri, "Should not happen.");

  ten_loc_init_app_uri_with_size(self, app_uri, strlen(app_uri));
}

void ten_loc_init_graph_id_with_size(ten_loc_t *self, const char *graph_id,
                                     size_t graph_id_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(graph_id, "Should not happen.");

  ten_string_init_from_c_str_with_size(&self->graph_id, graph_id, graph_id_len);
  self->has_graph_id = true;
}

void ten_loc_init_graph_id(ten_loc_t *self, const char *graph_id) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(graph_id, "Should not happen.");

  ten_loc_init_graph_id_with_size(self, graph_id, strlen(graph_id));
}

void ten_loc_init_extension_name_with_size(ten_loc_t *self,
                                           const char *extension_name,
                                           size_t extension_name_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(extension_name, "Should not happen.");

  ten_string_init_from_c_str_with_size(&self->extension_name, extension_name,
                                       extension_name_len);
  self->has_extension_name = true;
}

void ten_loc_init_extension_name(ten_loc_t *self, const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(extension_name, "Should not happen.");

  ten_loc_init_extension_name_with_size(self, extension_name,
                                        strlen(extension_name));
}

void ten_loc_set_app_uri_with_size(ten_loc_t *self, const char *app_uri,
                                   size_t app_uri_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(app_uri, "Should not happen.");

  ten_string_set_from_c_str_with_size(&self->app_uri, app_uri, app_uri_len);
  self->has_app_uri = true;
}

void ten_loc_set_app_uri(ten_loc_t *self, const char *app_uri) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(app_uri, "Should not happen.");

  ten_loc_set_app_uri_with_size(self, app_uri, strlen(app_uri));
}

void ten_loc_set_graph_id_with_size(ten_loc_t *self, const char *graph_id,
                                    size_t graph_id_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(graph_id, "Should not happen.");

  ten_string_set_from_c_str_with_size(&self->graph_id, graph_id, graph_id_len);
  self->has_graph_id = true;
}

void ten_loc_set_graph_id(ten_loc_t *self, const char *graph_id) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(graph_id, "Should not happen.");

  ten_loc_set_graph_id_with_size(self, graph_id, strlen(graph_id));
}

void ten_loc_set_extension_name_with_size(ten_loc_t *self,
                                          const char *extension_name,
                                          size_t extension_name_len) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(extension_name, "Should not happen.");

  ten_string_set_from_c_str_with_size(&self->extension_name, extension_name,
                                      extension_name_len);
  self->has_extension_name = true;
}

void ten_loc_set_extension_name(ten_loc_t *self, const char *extension_name) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_loc_check_integrity(self), "Should not happen.");
  TEN_ASSERT(extension_name, "Should not happen.");

  ten_loc_set_extension_name_with_size(self, extension_name,
                                       strlen(extension_name));
}

bool ten_loc_str_check_correct(const char *app_uri, const char *graph_id,
                               const char *extension_name, ten_error_t *err) {
  if (!app_uri) {
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                    "App URI cannot be empty.");
    }
    return false;
  } else {
    if (extension_name) {
      if (!graph_id) {
        if (err) {
          ten_error_set(err, TEN_ERROR_CODE_INVALID_ARGUMENT,
                        "Graph ID cannot be empty when extension name is "
                        "provided.");
        }
        return false;
      }
    }
  }
  return true;
}
