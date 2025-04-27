//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_predefined_graph : public ten::extension_t {
 public:
  explicit test_predefined_graph(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    nlohmann::json const detail = {{"id", 1}, {"name", "a"}};

    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property_from_json("detail", detail.dump().c_str());
    ten_env.return_result(std::move(cmd_result));
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
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
                 R"###({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "log": {
                          "level": 2
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": true,
                          "singleton": true,
                          "nodes": [{
                            "type":  "extension",
                            "name": "incorrect_extension",
                            "addon": "incorrect_addon",
                            "extension_group": "predefined_graph_group"
                          },{
                            "type":  "extension",
                            "name": "normal_extension",
                            "addon": "predefined_graph_incorrect_3__predefined_graph",
                            "extension_group": "predefined_graph_group"
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

void *app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    predefined_graph_incorrect_3__predefined_graph, test_predefined_graph);

}  // namespace

TEST(GraphErrorTest, PredefinedGraphIncorrect3) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  ten_thread_join(app_thread, -1);
}
