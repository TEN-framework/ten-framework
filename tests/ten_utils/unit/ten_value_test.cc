//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstring>

#include "gtest/gtest.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/value/value.h"
#include "ten_utils/value/value_get.h"
#include "ten_utils/value/value_is.h"
#include "ten_utils/value/value_json.h"
#include "ten_utils/value/value_object.h"

TEST(ValueTest, FromJsonStr_SimpleTypes) {  // NOLINT
  // Test with integer value
  {
    ten_value_t *value = ten_value_from_json_str("42");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(ten_value_is_uint64(value));
    EXPECT_EQ(42, ten_value_get_uint64(value, nullptr));
    ten_value_destroy(value);
  }

  // Test with boolean value
  {
    ten_value_t *value = ten_value_from_json_str("true");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(ten_value_is_bool(value));
    EXPECT_TRUE(ten_value_get_bool(value, nullptr));
    ten_value_destroy(value);
  }

  // Test with string value
  {
    ten_value_t *value = ten_value_from_json_str("\"hello\"");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(ten_value_is_string(value));
    ten_error_t err;
    TEN_ERROR_INIT(err);
    EXPECT_STREQ("hello", ten_value_peek_raw_str(value, &err));
    ten_value_destroy(value);
    ten_error_deinit(&err);
  }

  // Test with null value
  {
    ten_value_t *value = ten_value_from_json_str("null");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(ten_value_is_null(value));
    ten_value_destroy(value);
  }

  // Test with float value
  {
    ten_value_t *value = ten_value_from_json_str("3.14");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(ten_value_is_float64(value));
    EXPECT_FLOAT_EQ(3.14, ten_value_get_float64(value, nullptr));
    ten_value_destroy(value);
  }
}

TEST(ValueTest, FromJsonStr_Array) {  // NOLINT
  const char *json_str = "[1, true, \"hello\", 3.14, null]";
  ten_value_t *value = ten_value_from_json_str(json_str);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(ten_value_is_array(value));

  // Check array size.
  EXPECT_EQ(5, ten_value_array_size(value));

  // Check array elements.
  ten_error_t err;
  TEN_ERROR_INIT(err);

  size_t i = 0;
  ten_value_array_foreach(value, iter) {
    auto *item = static_cast<ten_value_t *>(ten_ptr_listnode_get(iter.node));
    ASSERT_NE(item, nullptr);

    switch (i) {
    case 0:
      EXPECT_TRUE(ten_value_is_uint64(item));
      EXPECT_EQ(1, ten_value_get_uint64(item, &err));
      break;
    case 1:
      EXPECT_TRUE(ten_value_is_bool(item));
      EXPECT_TRUE(ten_value_get_bool(item, &err));
      break;
    case 2:
      EXPECT_TRUE(ten_value_is_string(item));
      EXPECT_STREQ("hello", ten_value_peek_raw_str(item, &err));
      break;
    case 3:
      EXPECT_TRUE(ten_value_is_float64(item));
      EXPECT_FLOAT_EQ(3.14, ten_value_get_float64(item, &err));
      break;
    case 4:
      EXPECT_TRUE(ten_value_is_null(item));
      break;
    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    i++;
  }

  ten_value_destroy(value);
  ten_error_deinit(&err);
}

TEST(ValueTest, FromJsonStr_Object) {  // NOLINT
  const char *json_str =
      R"({"int": 42, "bool": true, "string": "hello", "float": 3.14, "null": null})";
  ten_value_t *value = ten_value_from_json_str(json_str);
  ASSERT_NE(value, nullptr);
  EXPECT_TRUE(ten_value_is_object(value));

  // Check object fields.
  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Check int field.
  ten_value_t *int_val = ten_value_object_peek(value, "int");
  ASSERT_NE(int_val, nullptr);
  EXPECT_TRUE(ten_value_is_uint64(int_val));
  EXPECT_EQ(42, ten_value_get_uint64(int_val, &err));

  // Check bool field.
  ten_value_t *bool_val = ten_value_object_peek(value, "bool");
  ASSERT_NE(bool_val, nullptr);
  EXPECT_TRUE(ten_value_is_bool(bool_val));
  EXPECT_TRUE(ten_value_get_bool(bool_val, &err));

  // Check string field.
  ten_value_t *string_val = ten_value_object_peek(value, "string");
  ASSERT_NE(string_val, nullptr);
  EXPECT_TRUE(ten_value_is_string(string_val));
  EXPECT_STREQ("hello", ten_value_peek_raw_str(string_val, &err));

  // Check float field.
  ten_value_t *float_val = ten_value_object_peek(value, "float");
  ASSERT_NE(float_val, nullptr);
  EXPECT_TRUE(ten_value_is_float64(float_val));
  EXPECT_FLOAT_EQ(3.14, ten_value_get_float64(float_val, &err));

  // Check null field.
  ten_value_t *null_val = ten_value_object_peek(value, "null");
  ASSERT_NE(null_val, nullptr);
  EXPECT_TRUE(ten_value_is_null(null_val));

  ten_value_destroy(value);
  ten_error_deinit(&err);
}

TEST(ValueTest, SetFromJsonStr_SimpleTypes) {  // NOLINT
  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Test setting int value.
  {
    ten_value_t *value = ten_value_create_int64(0);
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "42");
    EXPECT_TRUE(success);
    EXPECT_EQ(42, ten_value_get_int64(value, &err));
    ten_value_destroy(value);
  }

  // Test setting bool value.
  {
    ten_value_t *value = ten_value_create_bool(false);
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "true");
    EXPECT_TRUE(success);
    EXPECT_TRUE(ten_value_get_bool(value, &err));
    ten_value_destroy(value);
  }

  // Test setting string value.
  {
    ten_value_t *value = ten_value_create_string("");
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "\"hello\"");
    EXPECT_TRUE(success);
    EXPECT_STREQ("hello", ten_value_peek_raw_str(value, &err));
    ten_value_destroy(value);
  }

  // Test setting float value.
  {
    ten_value_t *value = ten_value_create_float64(0.0);
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "3.14");
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(3.14, ten_value_get_float64(value, &err));
    ten_value_destroy(value);
  }

  ten_error_deinit(&err);
}

TEST(ValueTest, SetFromJsonStr_Array) {  // NOLINT
  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Create an empty array.
  ten_list_t array = TEN_LIST_INIT_VAL;
  ten_value_t *value = ten_value_create_array_with_move(&array);
  ASSERT_NE(value, nullptr);

  // Set array from JSON string.
  const char *json_str = "[1, true, \"hello\"]";
  bool success = ten_value_set_from_json_str(value, json_str);
  EXPECT_TRUE(success);

  // Verify array contents.
  EXPECT_EQ(3, ten_value_array_size(value));

  size_t i = 0;
  ten_value_array_foreach(value, iter) {
    auto *item = static_cast<ten_value_t *>(ten_ptr_listnode_get(iter.node));
    ASSERT_NE(item, nullptr);

    switch (i) {
    case 0:
      EXPECT_TRUE(ten_value_is_uint64(item));
      EXPECT_EQ(1, ten_value_get_uint64(item, &err));
      break;
    case 1:
      EXPECT_TRUE(ten_value_is_bool(item));
      EXPECT_TRUE(ten_value_get_bool(item, &err));
      break;
    case 2:
      EXPECT_TRUE(ten_value_is_string(item));
      EXPECT_STREQ("hello", ten_value_peek_raw_str(item, &err));
      break;
    default:
      TEN_ASSERT(0, "Should not happen.");
      break;
    }
    i++;
  }

  ten_value_destroy(value);
  ten_error_deinit(&err);
}

TEST(ValueTest, SetFromJsonStr_Object) {  // NOLINT
  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Create an empty object.
  ten_list_t object = TEN_LIST_INIT_VAL;
  ten_value_t *value = ten_value_create_object_with_move(&object);
  ASSERT_NE(value, nullptr);

  // Set object from JSON string.
  const char *json_str = R"({"int": 42, "bool": true, "string": "hello"})";
  bool success = ten_value_set_from_json_str(value, json_str);
  EXPECT_TRUE(success);

  // Verify object contents.
  ten_value_t *int_val = ten_value_object_peek(value, "int");
  ASSERT_NE(int_val, nullptr);
  EXPECT_TRUE(ten_value_is_uint64(int_val));
  EXPECT_EQ(42, ten_value_get_uint64(int_val, &err));

  ten_value_t *bool_val = ten_value_object_peek(value, "bool");
  ASSERT_NE(bool_val, nullptr);
  EXPECT_TRUE(ten_value_is_bool(bool_val));
  EXPECT_TRUE(ten_value_get_bool(bool_val, &err));

  ten_value_t *string_val = ten_value_object_peek(value, "string");
  ASSERT_NE(string_val, nullptr);
  EXPECT_TRUE(ten_value_is_string(string_val));
  EXPECT_STREQ("hello", ten_value_peek_raw_str(string_val, &err));

  ten_value_destroy(value);
  ten_error_deinit(&err);
}

// Test type mismatch cases where the source JSON type doesn't match the target
// value type.
TEST(ValueTest, SetFromJsonStr_TypeMismatch) {  // NOLINT
  // Int to string conversion should fail.
  {
    ten_value_t *value = ten_value_create_string("");
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "42");
    EXPECT_FALSE(success);  // Should fail due to type mismatch.
    ten_value_destroy(value);
  }

  // String to int conversion should fail.
  {
    ten_value_t *value = ten_value_create_int64(0);
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "\"hello\"");
    EXPECT_FALSE(success);  // Should fail due to type mismatch.
    ten_value_destroy(value);
  }

  // Bool to float conversion should fail.
  {
    ten_value_t *value = ten_value_create_float64(0.0);
    ASSERT_NE(value, nullptr);
    bool success = ten_value_set_from_json_str(value, "true");
    EXPECT_FALSE(success);  // Should fail due to type mismatch.
    ten_value_destroy(value);
  }
}

TEST(ValueTest, SetFromJsonStr_NestedStructures) {  // NOLINT
  ten_error_t err;
  TEN_ERROR_INIT(err);

  // Create an empty object.
  ten_list_t object = TEN_LIST_INIT_VAL;
  ten_value_t *value = ten_value_create_object_with_move(&object);
  ASSERT_NE(value, nullptr);

  // Set a complex nested structure.
  const char *json_str = R"({
    "name": "test",
    "properties": {
      "value": 42,
      "enabled": true
    },
    "tags": ["a", "b", "c"],
    "metadata": {
      "created": 1625097600,
      "items": [
        {"id": 1, "name": "item1"},
        {"id": 2, "name": "item2"}
      ]
    }
  })";

  bool success = ten_value_set_from_json_str(value, json_str);
  EXPECT_TRUE(success);

  // Verify top-level fields.
  ten_value_t *name_val = ten_value_object_peek(value, "name");
  ASSERT_NE(name_val, nullptr);
  EXPECT_TRUE(ten_value_is_string(name_val));
  EXPECT_STREQ("test", ten_value_peek_raw_str(name_val, &err));

  // Verify nested object.
  ten_value_t *props_val = ten_value_object_peek(value, "properties");
  ASSERT_NE(props_val, nullptr);
  EXPECT_TRUE(ten_value_is_object(props_val));

  ten_value_t *value_val = ten_value_object_peek(props_val, "value");
  ASSERT_NE(value_val, nullptr);
  EXPECT_TRUE(ten_value_is_uint64(value_val));
  EXPECT_EQ(42, ten_value_get_uint64(value_val, &err));

  ten_value_t *enabled_val = ten_value_object_peek(props_val, "enabled");
  ASSERT_NE(enabled_val, nullptr);
  EXPECT_TRUE(ten_value_is_bool(enabled_val));
  EXPECT_TRUE(ten_value_get_bool(enabled_val, &err));

  // Verify array.
  ten_value_t *tags_val = ten_value_object_peek(value, "tags");
  ASSERT_NE(tags_val, nullptr);
  EXPECT_TRUE(ten_value_is_array(tags_val));
  EXPECT_EQ(3, ten_value_array_size(tags_val));

  // Verify deeply nested structure.
  ten_value_t *metadata_val = ten_value_object_peek(value, "metadata");
  ASSERT_NE(metadata_val, nullptr);
  EXPECT_TRUE(ten_value_is_object(metadata_val));

  ten_value_t *created_val = ten_value_object_peek(metadata_val, "created");
  ASSERT_NE(created_val, nullptr);
  EXPECT_TRUE(ten_value_is_uint64(created_val));
  EXPECT_EQ(1625097600, ten_value_get_uint64(created_val, &err));

  ten_value_t *items_val = ten_value_object_peek(metadata_val, "items");
  ASSERT_NE(items_val, nullptr);
  EXPECT_TRUE(ten_value_is_array(items_val));
  EXPECT_EQ(2, ten_value_array_size(items_val));

  ten_value_destroy(value);
  ten_error_deinit(&err);
}
