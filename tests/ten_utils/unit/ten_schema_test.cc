//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_utils/schema/schema.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/json.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_json.h"
#include "ten_utils/value/value_object.h"

static ten_schema_t *create_ten_schema_from_string(
    const std::string &schema_str) {
  auto *schema_json = ten_json_from_string(schema_str.c_str(), nullptr);
  auto *schema = ten_schema_create_from_json(schema_json);

  ten_json_destroy(schema_json);

  return schema;
}

TEST(SchemaTest, ValidStringType) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "string"
         })";

  auto *schema = create_ten_schema_from_string(schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  auto *str_value = ten_value_create_string("demo");

  bool success = ten_schema_validate_value(schema, str_value, &err);
  ASSERT_EQ(success, true);

  ten_value_destroy(str_value);

  auto *int_value = ten_value_create_int8(1);

  success = ten_schema_validate_value(schema, int_value, &err);
  ASSERT_EQ(success, false);
  ASSERT_EQ(ten_error_is_success(&err), false);

  // the value type does not match the schema type, given: int8, expected:
  // string
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg,
            "the value type does not match the schema type, given: int8, "
            "expected: string");

  ten_value_destroy(int_value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, ValidObjectType) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "object",
           "properties": {
             "name": {
               "type": "string"
             },
             "age": {
               "type": "int64"
             }
           },
           "required": ["name"]
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"({
                                   "name": "demo",
                                   "age": 18
                                 })";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_validate_value(schema, value, &err);
  ASSERT_EQ(success, true);

  ten_value_destroy(value);

  auto *invalid_json = ten_json_from_string(R"({
    "name": 11,
    "age": 18
  })",
                                            nullptr);
  auto *invalid_value = ten_value_from_json(invalid_json);
  ten_json_destroy(invalid_json);

  success = ten_schema_validate_value(schema, invalid_value, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(invalid_value);

  // .name: the value type does not match the schema type, given: uint64,
  // expected: string
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind(".name:", 0) == 0, true);

  // Testing for required.
  ten_error_reset(&err);

  auto *missing_json = ten_json_from_string(R"({
    "age": 18
  })",
                                            nullptr);
  auto *missing_value = ten_value_from_json(missing_json);
  ten_json_destroy(missing_json);

  success = ten_schema_validate_value(schema, missing_value, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(missing_value);

  // the required properties are absent: 'name'
  err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg, "the required properties are absent: 'name'");

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, CompositeObjectValidateErrMsg) {  // NOLINT
  const std::string schema_str =
      R"({
            "type": "object",
            "properties": {
              "a": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": {
                    "b": {
                      "type": "int64"
                    },
                    "c": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    },
                    "d": {
                      "type": "object",
                      "properties": {
                        "e": {
                          "type": "int64"
                        },
                        "f": {
                          "type": "buf"
                        }
                      },
                      "required": ["e", "f"]
                    }
                  }
                }
              }
            }
          })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"({
                                    "a": [
                                      {
                                        "b": 1,
                                        "c": [
                                          "1",
                                          2
                                        ]
                                      }
                                    ]
                                  })";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_validate_value(schema, value, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(value);

  // .a[0].c[1]: the value type does not match the schema type, given: uint64,
  // expected: string
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind(".a[0].c[1]", 0), 0);

  ten_error_reset(&err);

  const std::string value_str2 = R"({
                                     "a": [
                                       {
                                         "b": 1,
                                         "c": [
                                           "1",
                                           "2"
                                         ],
                                         "d": {
                                           "e": 1
                                         }
                                       }
                                     ]
                                   })";
  auto *value_json2 = ten_json_from_string(value_str2.c_str(), nullptr);
  auto *value2 = ten_value_from_json(value_json2);
  ten_json_destroy(value_json2);

  success = ten_schema_validate_value(schema, value2, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(value2);

  // .a[0].d: the required properties are absent: 'f'
  err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind(".a[0].d:", 0), 0);

  ten_error_deinit(&err);

  ten_schema_destroy(schema);
}

TEST(SchemaTest, RequiredErrorMessage) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "object",
           "properties": {
             "name": {
               "type": "string"
             },
             "body": {
                "type": "object",
                "properties": {
                  "height": {
                    "type": "float32"
                  },
                  "weight": {
                    "type": "float32"
                  }
                },
                "required": ["height", "weight"]
             }
           },
           "required": ["body"]
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"({
                                   "name": "demo",
                                   "body": {}
                                 })";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_validate_value(schema, value, &err);
  ASSERT_EQ(success, false);

  // .body: the required properties are absent: 'height', 'weight'
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind(".body:", 0) == 0, true);

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, ValidArrayType) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int64"
           }
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"([1, 2, 3])";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_validate_value(schema, value, &err);
  ASSERT_EQ(success, true);

  auto *invalid_json = ten_json_from_string(R"([1, "2", 3])", nullptr);
  auto *invalid_value = ten_value_from_json(invalid_json);
  ten_json_destroy(invalid_json);

  success = ten_schema_validate_value(schema, invalid_value, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(invalid_value);

  // [1]: the value type does not match the schema type, given: string,
  // expected: int64
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind("[1]:", 0) == 0, true);

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, AdjustIntValue) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "int64"
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  auto *value = ten_value_create_int8(1);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_adjust_value_type(schema, value, &err);
  ASSERT_EQ(success, true);

  ASSERT_TRUE(ten_value_is_int64(value));
  ASSERT_EQ(1, ten_value_get_int64(value, &err));

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, AdjustObject) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "object",
           "properties": {
             "name": {
               "type": "string"
             },
             "age": {
               "type": "uint8"
             }
           }
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"({
                                   "name": "demo",
                                   "age": 18
                                 })";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  auto *value_age = ten_value_object_peek(value, "age");
  ASSERT_EQ(true, ten_value_get_uint8(value_age, &err) == 18);

  bool success = ten_schema_adjust_value_type(schema, value, &err);
  ASSERT_EQ(success, true);

  ASSERT_TRUE(ten_value_is_uint8(value_age));
  ASSERT_EQ(18, ten_value_get_uint8(value_age, &err));

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, AdjustArray) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int32"
           }
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"([1, 2, 3])";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  auto *value_one = ten_value_array_peek(value, 0, &err);
  ASSERT_EQ(true, ten_value_get_int32(value_one, &err) == 1);

  bool success = ten_schema_adjust_value_type(schema, value, &err);
  ASSERT_EQ(success, true);

  ASSERT_EQ(true, ten_value_is_int32(value_one));
  ASSERT_EQ(1, ten_value_get_int32(value_one, &err));

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, Required) {  // NOLINT
  const std::string schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           },
           "required": ["a"]
         })";
  auto *schema = create_ten_schema_from_string(schema_str);

  const std::string value_str = R"({
                                     "b": 18
                                   })";
  auto *value_json = ten_json_from_string(value_str.c_str(), nullptr);
  auto *value = ten_value_from_json(value_json);

  ten_json_destroy(value_json);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_adjust_value_type(schema, value, &err);
  ASSERT_EQ(success, true);

  success = ten_schema_validate_value(schema, value, &err);
  ASSERT_EQ(success, false);

  ten_value_destroy(value);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}

TEST(SchemaTest, CompatibleIntSuccess) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "int32"
         })";

  const std::string target_schema_str =
      R"({
           "type": "int64"
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleIntFail) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "int32"
         })";

  const std::string target_schema_str =
      R"({
           "type": "string"
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, false);

  // type is incompatible, source is [int32], but target is [string]
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg,
            "type is incompatible, source is [int32], but target is [string]");

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleProperties) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatiblePropertiesSuperSet) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             }
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatiblePropertiesSubset) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             }
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatiblePropertiesMismatchType) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "int8"
             },
             "c": {
               "type": "uint8"
             }
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, false);

  // .a: type is incompatible, source is [string], but target is [int8]
  auto err_msg = std::string(ten_error_message(&err));
  ASSERT_EQ(err_msg.rfind("{ .a:", 0) == 0, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleRequired) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           },
           "required": ["a"]
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           },
           "required": ["a"]
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleRequiredSubset) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           },
           "required": ["a"]
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           },
           "required": ["a", "b"]
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, false);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleRequiredSuperset) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           },
           "required": ["a", "b"]
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           },
           "required": ["a"]
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleRequiredSourceUndefined) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           },
           "required": ["a"]
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, false);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleRequiredTargetUndefined) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint8"
             }
           },
           "required": ["a"]
         })";

  const std::string target_schema_str =
      R"({
           "type": "object",
           "properties": {
             "a": {
               "type": "string"
             },
             "b": {
               "type": "uint16"
             }
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleItems) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int32"
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int64"
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompatibleItems2) {  // NOLINT
  const std::string source_schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int32"
           }
         })";

  const std::string target_schema_str =
      R"({
           "type": "array",
           "items": {
             "type": "int8"
           }
         })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, true);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, CompositeObjectCompatibleFail) {  // NOLINT
  const std::string source_schema_str =
      R"({
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "a": {
                  "type": "array",
                  "items": {
                    "type": "int32"
                  }
                },
                "b": {
                  "type": "buf"
                },
                "c": {
                  "type": "object",
                  "properties": {
                    "d": {
                      "type": "int32"
                    },
                    "e": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    }
                  }
                }
              }
            }
          })";

  const std::string target_schema_str =
      R"({
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "a": {
                  "type": "array",
                  "items": {
                    "type": "string"
                  }
                },
                "b": {
                  "type": "string"
                },
                "c": {
                  "type": "object",
                  "properties": {
                    "d": {
                      "type": "float32"
                    },
                    "e": {
                      "type": "array",
                      "items": {
                        "type": "ptr"
                      }
                    }
                  }
                }
              }
            }
          })";

  auto *source_schema = create_ten_schema_from_string(source_schema_str);
  auto *target_schema = create_ten_schema_from_string(target_schema_str);

  ten_error_t err;
  TEN_ERROR_INIT(err);

  // []: { .a[]: type is incompatible, source is [int32], but target is
  // [string]; .b: type is incompatible, source is [buf], but target is
  // [string]; .c: { .d: type is incompatible, source is [int32], but target is
  // [float32]; .e[]: type is incompatible, source is [string], but target is
  // [ptr] } }
  bool success = ten_schema_is_compatible(source_schema, target_schema, &err);
  ASSERT_EQ(success, false);

  ten_error_deinit(&err);

  ten_schema_destroy(source_schema);
  ten_schema_destroy(target_schema);
}

TEST(SchemaTest, PathInfoInErrMsg) {  // NOLINT
  const std::string schema_str = R"({
    "type": "object",
    "properties": {
      "a": {
        "type": "string"
      },
      "b": {
        "type": "array",
        "items": {
          "type": "string"
        }
      },
      "c": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "d": {
              "type": "int32"
            }
          }
        }
      }
    }
  })";

  ten_error_t err;
  TEN_ERROR_INIT(err);

  auto *schema = create_ten_schema_from_string(schema_str);

  // Testing for a.
  auto *json_a = ten_json_from_string(R"({
    "a": 1
  })",
                                      nullptr);
  auto *value_a = ten_value_from_json(json_a);
  ten_json_destroy(json_a);

  ten_schema_adjust_value_type(schema, value_a, &err);

  ten_value_destroy(value_a);

  // .a: unsupported conversion from `uint64` to `string`
  auto err_a = std::string(ten_error_message(&err));
  ASSERT_EQ(err_a.rfind(".a", 0) == 0, true);

  // Testing for b.
  ten_error_reset(&err);

  auto *json_b = ten_json_from_string(R"({
    "b": ["1", 2, "3"]
  })",
                                      nullptr);
  auto *value_b = ten_value_from_json(json_b);
  ten_json_destroy(json_b);

  ten_schema_adjust_value_type(schema, value_b, &err);

  ten_value_destroy(value_b);

  // .b[1]: unsupported conversion from `uint64` to `string`
  auto err_b = std::string(ten_error_message(&err));
  ASSERT_EQ(err_b.rfind(".b[1]", 0) == 0, true);

  // Testing for c.
  ten_error_reset(&err);

  auto *json_c = ten_json_from_string(R"({
    "c": [
      {
        "d": "1"
      },
      {
        "d": 2
      },
      {
        "d": "3"
      }
    ]
  })",
                                      nullptr);
  auto *value_c = ten_value_from_json(json_c);
  ten_json_destroy(json_c);

  ten_schema_adjust_value_type(schema, value_c, &err);

  ten_value_destroy(value_c);

  // .c[0].d: unsupported conversion from `string` to `int32`
  auto err_c = std::string(ten_error_message(&err));
  ASSERT_EQ(err_c.rfind(".c[0].d", 0) == 0, true);

  ten_error_deinit(&err);
  ten_schema_destroy(schema);
}
