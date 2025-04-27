//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <libwebsockets.h>

#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/http.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "hello world, too");
      ten_env.return_result(std::move(cmd_result));
    }
  }
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    http_server_extension_close_app__test_extension, test_extension);

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) final {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env,
        // clang-format off
                 R"({
                      "type": "app",
                      "name": "test_app",
                      "version": "0.1.0"
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "log": {
                          "level": 2
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "simple_http_server_cpp",
                            "addon": "simple_http_server_cpp",
                            "extension_group": "test_extension_group"
                          },{
                            "type": "extension",
                            "name": "test_extension",
                            "addon": "http_server_extension_close_app__test_extension",
                            "extension_group": "test_extension_group"
                          }],
                          "connections": [{
                            "extension": "simple_http_server_cpp",
                            "cmd": [{
                              "name": "hello_world",
                              "dest": [{
                                "extension": "test_extension"
                              }]
                            }]
                          }]
                        }]
                      }
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

ten::app_t *test_app_ = nullptr;

void *test_app_thread_main(TEN_UNUSED void *args) {
  test_app_ = new test_app();
  test_app_->run(true);
  test_app_->wait();
  delete test_app_;
  test_app_ = nullptr;

  return nullptr;
}

}  // namespace

TEST(ExtensionTest, HttpServerExtensionCloseApp) {  // NOLINT
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  ten_test_http_client_init();

  ten_string_t resp;
  TEN_STRING_INIT(resp);
  ten_test_http_client_post("http://127.0.0.1:8001/",
                            R"({"ten": {"name": "hello_world"}})", &resp);
  EXPECT_EQ(std::string(ten_string_get_raw_str(&resp)), "\"hello world, too\"");

  ten_string_clear(&resp);
  ten_test_http_client_post("http://127.0.0.1:8001/",
                            R"({"ten": {"type": "close_app"}})", &resp);
  EXPECT_EQ(std::string(ten_string_get_raw_str(&resp)), "TEN is closed.");

  ten_string_deinit(&resp);
  ten_test_http_client_deinit();

  ten_thread_join(app_thread, -1);
}
