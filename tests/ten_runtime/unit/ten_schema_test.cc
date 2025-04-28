//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/schema_store/store.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_json.h"

TEST(SchemaTest, SchemaStoreValidateProperty) {  // NOLINT
  const std::string schema_str = R"({
    "property": {
      "name": {
        "type": "string"
      },
      "age": {
        "type": "int64"
      }
    },
    "cmd_in": [
      {
        "name": "hello",
        "property": {
          "count": {
            "type": "int32"
          }
        },
        "result": {
          "ten": {
            "detail": {
              "type": "string"
            }
          }
        }
      }
    ],
    "data_in": [
      {
        "name": "data",
        "property": {
          "fps": {
            "type": "int16"
          }
        }
      }
    ]
  })";

  auto *schema_json = ten_json_from_string(schema_str.c_str(), nullptr);
  auto *schema_value = ten_value_from_json(schema_json);
  ten_json_destroy(schema_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  ten_schema_store_t schema_store;
  ten_schema_store_init(&schema_store);
  ten_schema_store_set_schema_definition(&schema_store, schema_value, &err);
  ten_value_destroy(schema_value);

  const std::string properties_str = R"({
    "name": "demo",
    "age": 18
  })";
  auto *properties_json = ten_json_from_string(properties_str.c_str(), nullptr);
  auto *properties_value = ten_value_from_json(properties_json);
  ten_json_destroy(properties_json);

  bool success = ten_schema_store_validate_properties(&schema_store,
                                                      properties_value, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);
  ten_value_destroy(properties_value);

  ten_schema_store_deinit(&schema_store);
}
