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

  void on_start(ten::ten_env_t &ten_env) override {
    ten_env.on_start_done();

    auto test_cmd = ten::cmd_t::create("test_cmd_from_1");
    test_cmd->set_property_from_json(nullptr, R"({
      "string_field": "hello world",
      "int_field": 42,
      "float_field": 3.14159,
      "bool_field": true,
      "negative_int": -100,
      "large_number": 9223372036854775807
    })",
                                     nullptr);
    ten_env.send_cmd(std::move(test_cmd));
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test_cmd_from_2") {
      auto cmd_json_str = cmd->get_property_to_json();
      ten_json_t *cmd_json =
          ten_json_from_string(cmd_json_str.c_str(), nullptr);

      ten::value_t fields;
      fields.from_json(cmd_json);
      ten_json_destroy(cmd_json);

      TEN_ENV_LOG(ten_env, TEN_LOG_LEVEL_INFO,
                  "test_cmd_from_2 received with detailed fields", nullptr,
                  &fields);

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      ten_env.return_result(std::move(cmd_result));

      ten_random_sleep_range_ms(1000, 2000);

      if (cnt < 100) {
        cnt++;

        TEN_ENV_LOG_INFO(ten_env, "test_cmd_from_1 sent");
        auto test_cmd = ten::cmd_t::create("test_cmd_from_1");
        test_cmd->set_property_from_json(nullptr, R"({
          "string_field": "hello world",
          "int_field": 42,
          "float_field": 3.14159,
          "bool_field": true,
          "negative_int": -100,
          "large_number": 9223372036854775807
        })");
        ten_env.send_cmd(std::move(test_cmd));
      } else {
        auto close_app = ten::close_app_cmd_t::create();
        close_app->set_dests({{""}});
        ten_env.send_cmd(std::move(close_app));
      }
    }
  }

 private:
  size_t cnt{0};
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(extension_1, test_extension);
