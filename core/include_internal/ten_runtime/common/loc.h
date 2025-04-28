//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <stdbool.h>

#include "ten_utils/lib/string.h"
#include "ten_utils/value/value.h"

#define TEN_LOC_SIGNATURE 0x581B639EF70CBC5DU

typedef struct ten_extension_t ten_extension_t;

// This type represents the dynamic information of a extension. Do not mix
// static information of a extension here.
//
// - dynamic information
//   Something like the 'object' information in an object-oriented programming
//   language, which is how to 'locate' the object instance.
//   Therefore, the dynamic information of a extension is the information
//   relevant to the location of a extension instance. Ex: the uri of the app,
//   the graph_id of the engine, the name of the extension group and the
//   extension.
//
// - static information
//   Something like the 'type' information in an object-oriented programming
//   language, which is how to 'create' the object instance.
//   Therefore, the static information of a extension is the information
//   relevant to the creating logic of a extension instance. Ex: the addon
//   name of the extension group and the extension.
typedef struct ten_loc_t {
  ten_signature_t signature;

  ten_string_t app_uri;
  ten_string_t graph_id;
  ten_string_t extension_group_name;
  ten_string_t extension_name;
} ten_loc_t;

TEN_RUNTIME_PRIVATE_API bool ten_loc_check_integrity(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_loc_create_empty(void);

TEN_RUNTIME_API ten_loc_t *ten_loc_create(const char *app_uri,
                                          const char *graph_id,
                                          const char *extension_group_name,
                                          const char *extension_name);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_loc_create_from_value(
    ten_value_t *value);

TEN_RUNTIME_API void ten_loc_destroy(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API ten_loc_t *ten_loc_clone(ten_loc_t *src);

TEN_RUNTIME_PRIVATE_API void ten_loc_copy(ten_loc_t *self, ten_loc_t *src);

TEN_RUNTIME_PRIVATE_API void ten_loc_init_empty(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API void ten_loc_init(ten_loc_t *self, const char *app_uri,
                                          const char *graph_id,
                                          const char *extension_group_name,
                                          const char *extension_name);

TEN_RUNTIME_PRIVATE_API void ten_loc_init_from_loc(ten_loc_t *self,
                                                   ten_loc_t *src);

TEN_RUNTIME_PRIVATE_API void ten_loc_init_from_value(ten_loc_t *self,
                                                     ten_value_t *value);

TEN_RUNTIME_PRIVATE_API void ten_loc_deinit(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API void ten_loc_set(ten_loc_t *self, const char *app_uri,
                                         const char *graph_id,
                                         const char *extension_group_name,
                                         const char *extension_name);

TEN_RUNTIME_PRIVATE_API void ten_loc_set_from_loc(ten_loc_t *self,
                                                  ten_loc_t *src);

TEN_RUNTIME_PRIVATE_API void ten_loc_set_from_value(ten_loc_t *self,
                                                    ten_value_t *value);

TEN_RUNTIME_PRIVATE_API bool ten_loc_is_empty(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API void ten_loc_clear(ten_loc_t *self);

TEN_RUNTIME_PRIVATE_API bool ten_loc_is_equal(ten_loc_t *self,
                                              ten_loc_t *other);

TEN_RUNTIME_PRIVATE_API bool ten_loc_is_equal_with_value(
    ten_loc_t *self, const char *app_uri, const char *graph_id,
    const char *extension_group_name, const char *extension_name);

TEN_RUNTIME_PRIVATE_API void ten_loc_to_string(ten_loc_t *self,
                                               ten_string_t *result);

TEN_RUNTIME_PRIVATE_API ten_value_t *ten_loc_to_value(ten_loc_t *self);
