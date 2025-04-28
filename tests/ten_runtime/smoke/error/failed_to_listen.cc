//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    // The protocol (error://) is not a correct one, so the listening will fail.
    bool rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "error://127.0.0.1:8001/",
                        "log": {
                          "level": 2
                        }
                      }
                    })"
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

}  // namespace

TEST(ExtensionTest, FailedToListen) {  // NOLINT
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // The app would stop automatically after the listening failed.

  ten_thread_join(app_thread, -1);
}
