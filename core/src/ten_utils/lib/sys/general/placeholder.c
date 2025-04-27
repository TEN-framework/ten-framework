//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_utils/lib/placeholder.h"

#include <stdbool.h>
#include <string.h>

#include "include_internal/ten_utils/common/constant_str.h"
#include "include_internal/ten_utils/macro/memory.h"
#include "include_internal/ten_utils/value/value.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"
#include "ten_utils/macro/memory.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"

static bool ten_placeholder_check_integrity(ten_placeholder_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) !=
      (ten_signature_t)TEN_PLACEHOLDER_SIGNATURE) {
    return false;
  }

  return true;
}

bool ten_c_str_is_placeholder(const char *input) {
  TEN_ASSERT(input, "Invalid argument.");

  // Check if it starts with ${ and ends with }.
  if (strncmp(input, "${", 2) != 0 || input[strlen(input) - 1] != '}') {
    return false;
  }
  return true;
}

void ten_placeholder_init(ten_placeholder_t *self) {
  TEN_ASSERT(self, "Invalid argument.");

  ten_signature_set(&self->signature,
                    (ten_signature_t)TEN_PLACEHOLDER_SIGNATURE);
  self->scope = TEN_PLACEHOLDER_SCOPE_INVALID;

  ten_value_init_invalid(&self->default_value);
  TEN_STRING_INIT(self->variable);
}

ten_placeholder_t *ten_placeholder_create(void) {
  ten_placeholder_t *self = TEN_MALLOC(sizeof(ten_placeholder_t));
  TEN_ASSERT(self, "Failed to allocate memory.");

  ten_placeholder_init(self);

  return self;
}

void ten_placeholder_deinit(ten_placeholder_t *self) {
  TEN_ASSERT(self && ten_placeholder_check_integrity(self),
             "Invalid argument.");

  ten_string_deinit(&self->variable);
  ten_value_deinit(&self->default_value);
}

void ten_placeholder_destroy(ten_placeholder_t *self) {
  TEN_ASSERT(self && ten_placeholder_check_integrity(self),
             "Invalid argument.");

  ten_placeholder_deinit(self);

  TEN_FREE(self);
}

static TEN_PLACEHOLDER_SCOPE ten_placeholder_scope_from_string(
    const char *scope_str) {
  TEN_ASSERT(scope_str, "Invalid argument.");

  if (!strcmp(scope_str, TEN_STR_ENV)) {
    return TEN_PLACEHOLDER_SCOPE_ENV;
  } else {
    TEN_ASSERT(0, "Should not happen.");
    return TEN_PLACEHOLDER_SCOPE_INVALID;
  }
}

static char *ten_placeholder_escape_characters(const char *input) {
  TEN_ASSERT(input, "Invalid argument.");

  // Create a buffer to store the parsed string.
  size_t len = strlen(input);
  char *output = TEN_MALLOC(len + 1);
  char *out_ptr = output;
  bool escape = false;

  for (const char *ptr = input; *ptr != '\0'; ptr++) {
    if (*ptr == '\\' && !escape) {
      escape = true;
      continue;
    }

    *out_ptr++ = *ptr;
    escape = false;
  }

  *out_ptr = '\0';
  return output;
}

bool ten_placeholder_parse(ten_placeholder_t *self, const char *input,
                           ten_error_t *err) {
  TEN_ASSERT(self && ten_placeholder_check_integrity(self),
             "Invalid argument.");

  if (!ten_c_str_is_placeholder(input)) {
    return false;
  }

  // Remove `${` and `}` and create a temporary, modifiable copy.
  char *content = TEN_STRNDUP(input + 2, strlen(input) - 3);

  // Parse the scope part.
  char *scope_end = strchr(content, TEN_STR_PLACEHOLDER_SCOPE_DELIMITER);
  if (!scope_end) {
    TEN_FREE(content);
    return false;
  }

  *scope_end = '\0';

  self->scope = ten_placeholder_scope_from_string(content);

  // Parse the variable and default_value parts.
  char *variable_start = scope_end + 1;
  char *variable_end = NULL;

  char *escaped_variable = ten_placeholder_escape_characters(variable_start);
  variable_end = strchr(escaped_variable, '|');

  if (variable_end) {
    // Extract variable and default_value.
    *variable_end = '\0';

    ten_string_set_formatted(&self->variable, "%s", escaped_variable);

    if (*(variable_end + 1) == '\0') {
      ten_value_reset_to_string_with_size(&self->default_value, "", 0);
    } else {
      ten_value_reset_to_string_with_size(
          &self->default_value, variable_end + 1, strlen(variable_end + 1));
    }
  } else {
    // Extract only variable.
    ten_string_set_formatted(&self->variable, "%s", variable_start);
  }

  TEN_FREE(content);
  TEN_FREE(escaped_variable);

  return true;
}

bool ten_placeholder_resolve(ten_placeholder_t *self,
                             ten_value_t *placeholder_value, ten_error_t *err) {
  TEN_ASSERT(self && ten_placeholder_check_integrity(self),
             "Invalid argument.");
  TEN_ASSERT(placeholder_value != NULL, "Invalid argument.");

  switch (self->scope) {
  case TEN_PLACEHOLDER_SCOPE_ENV: {
    const char *variable_name = ten_string_get_raw_str(&self->variable);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *env_value = getenv(variable_name);

    if (env_value != NULL) {
      // Environment variable found, set the resolved value
      ten_value_reset_to_string_with_size(placeholder_value, env_value,
                                          strlen(env_value));
    } else {
      // Environment variable not found, use default value.
      if (!ten_value_is_valid(&self->default_value)) {
        // If no default value is provided, use 'null' value.
        TEN_LOGE(
            "Environment variable %s is not found, neither default value is "
            "provided.",
            variable_name);

        // For now, we are handling this issue with a simple direct `exit`.
        // This might not be too unusual. For example, in Java/Sprintbot, if a
        // key specified in `application.yaml` is set to be retrieved from an
        // environment variable, but the environment variable does not exist,
        // Sprintbot will fail to start and immediately exit the process.
        //
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        exit(EXIT_FAILURE);

        ten_value_reset_to_null(placeholder_value);
      } else {
        const char *default_value =
            ten_value_peek_raw_str(&self->default_value, err);

        TEN_LOGI(
            "Environment variable %s is not found, using default value %s.",
            variable_name, default_value);

        ten_value_reset_to_string_with_size(placeholder_value, default_value,
                                            strlen(default_value));
      }
    }
    break;
  }

  default:
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Unsupported placeholder scope: %d", self->scope);
    }
    TEN_ASSERT(0, "Should not happen.");
    return false;
  }

  return true;
}
