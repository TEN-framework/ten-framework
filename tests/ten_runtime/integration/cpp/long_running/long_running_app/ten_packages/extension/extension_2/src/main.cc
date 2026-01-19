//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cassert>
#include <cstdlib>

#include "ten_runtime/binding/cpp/ten.h"

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test_cmd_from_1") {
      auto cmd_json_str = cmd->get_property_to_json();

      ten::value_t fields;
      bool rc = fields.from_json(cmd_json_str.c_str());
      TEN_ASSERT(rc, "Should not happen.");

      TEN_ENV_LOG(ten_env, TEN_LOG_LEVEL_INFO,
                  "test_cmd_from_1 received with detailed fields", nullptr,
                  &fields);

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      ten_env.return_result(std::move(cmd_result));

      ten_random_sleep_range_ms(1000, 2000);

      TEN_ENV_LOG_INFO(ten_env, "test_cmd_from_2 sent");
      auto test_cmd = ten::cmd_t::create("test_cmd_from_2");

      test_cmd->set_property_from_json(nullptr, R"({
        "string_field": "test_cmd_from_2 hello world",
        "int_field": 43,
        "float_field": 3.1415926,
        "bool_field": false,
        "negative_int": -101,
        "large_number": 9223372036854775807
      })");
      ten_env.send_cmd(std::move(test_cmd));
    }
  }
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(extension_2, test_extension);
