//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      ten_env.send_cmd(
          std::move(cmd),
          [](ten::ten_env_t &ten_env,
             std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
            auto property_value = cmd_result->get_property_string(
                "resp_group.resp_property_name");

            cmd_result->set_property("detail", property_value);
            ten_env.return_result(std::move(cmd_result));
          });
    }
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_mapping") {
      if (cmd->get_property_int64("test_group.test_property_name") == 32) {
        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);

        cmd_result->set_property("detail", "hello world, too");
        cmd_result->set_property("resp_property", "may the force be with you.");

        ten_env.return_result(std::move(cmd_result));
      }
    }
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env,
        // clang-format off
                 R"###({
                   "type": "app",
                   "name": "test_app",
                   "version": "0.1.0"
                 })###"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"###({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "log": {
                          "level": 2
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": false,
                          "singleton": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "test_extension_1",
                            "addon": "result_mapping_1__test_extension_1",
                            "app": "msgpack://127.0.0.1:8001/",
                            "extension_group": "result_mapping_1__extension_group"
                          },{
                            "type": "extension",
                            "name": "test_extension_2",
                            "addon": "result_mapping_1__test_extension_2",
                            "app": "msgpack://127.0.0.1:8001/",
                            "extension_group": "result_mapping_1__extension_group"
                          }],
                          "connections": [{
                            "app": "msgpack://127.0.0.1:8001/",
                            "extension": "test_extension_1",
                            "cmd": [{
                              "name": "hello_world",
                              "dest": [{
                                "app": "msgpack://127.0.0.1:8001/",
                                "extension": "test_extension_2",
                                "msg_conversion": {
                                  "type": "per_property",
                                  "rules": [{
                                    "path": "ten.name",
                                    "conversion_mode": "fixed_value",
                                    "value": "hello_mapping"
                                  },{
                                    "path": "test_group.test_property_name",
                                    "conversion_mode": "from_original",
                                    "original_path": "test_property"
                                  }],
                                  "result": {
                                    "type": "per_property",
                                    "rules": [{
                                      "path": "resp_group.resp_property_name",
                                      "conversion_mode": "from_original",
                                      "original_path": "resp_property"
                                    }]
                                  }
                                }
                              }]
                            }]
                          }]
                        }]
                      }
                    })###"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }
};

void *test_app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(result_mapping_1__test_extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(result_mapping_1__test_extension_2,
                                    test_extension_2);

}  // namespace

TEST(ResultConversionTest, ResultConversion1) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                            "result_mapping_1__extension_group",
                            "test_extension_1");
  hello_world_cmd->set_property("test_property", 32);

  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(hello_world_cmd));

  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "may the force be with you.");

  delete client;

  ten_thread_join(app_thread, -1);
}
