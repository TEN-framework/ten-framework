//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "optional_value": null
           })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    ten_env.on_configure_done();
  }

  void on_start(ten::ten_env_t &ten_env) override {
    auto optional_value = ten_env.get_property_to_json("optional_value");
    auto optional_value_json = nlohmann::json::parse(optional_value);
    EXPECT_EQ(optional_value_json, nullptr);

    auto close_app = ten::close_app_cmd_t::create();
    close_app->set_dests({{""}});
    ten_env.send_cmd(std::move(close_app));
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "log": {
                 "handlers": [
                   {
                     "matchers": [
                       {
                         "level": "debug"
                       }
                     ],
                     "formatter": {
                       "type": "plain",
                       "colored": true
                     },
                     "emitter": {
                       "type": "console",
                       "config": {
                         "stream": "stdout"
                       }
                     }
                   }
                 ]
               },
               "predefined_graphs": [{
                 "name": "default",
                 "auto_start": true,
                 "graph": {
                   "nodes": [{
                     "type": "extension",
                     "name": "test_extension",
                     "addon": "optional_value__test_extension"
                   }]
                 }
               }]
             }
           })",
        // clang-format on
        nullptr);
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(optional_value__test_extension,
                                    test_extension);

}  // namespace

// Currently we don't support optional semantics in schema, so we cannot specify
// null. In the future we need to support this and enable this test case.
TEST(ValueTest, DISABLED_OptionalValue) {  // NOLINT
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  ten_thread_join(app_thread, -1);
}
