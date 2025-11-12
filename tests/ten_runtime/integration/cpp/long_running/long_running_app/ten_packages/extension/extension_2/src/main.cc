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
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      ten_env.return_result(std::move(cmd_result));

      ten_random_sleep_range_ms(1000, 2000);

      auto test_cmd = ten::cmd_t::create("test_cmd_from_2");
      ten_env.send_cmd(std::move(test_cmd));
    }
  }
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(extension_2, test_extension);
