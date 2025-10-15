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
    if (cmd->get_name() == "test") {
      // Cache the cmd.
      cmd_ = std::move(cmd);

      auto data = ten::data_t::create("text_data");
      data->set_property("text", "hello_world");
      bool rc = ten_env.send_data(std::move(data));
      // According to the interface schema, the property `text` should be an
      // int64. So the `rc` should be false.
      TEN_ASSERT(!rc, "rc should be false");

      data->set_property("text", 1234);
      rc = ten_env.send_data(std::move(data));
      TEN_ASSERT(rc, "rc should be true");
    }
  }

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    if (data->get_name() == "text_data") {
      TEN_ASSERT(cmd_ != nullptr, "cmd_ is nullptr");

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd_);
      cmd_result->set_property("detail", "data received");
      ten_env.return_result(std::move(cmd_result));
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> cmd_;
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(default_extension_cpp, test_extension);
