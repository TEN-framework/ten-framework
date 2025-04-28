//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_migration : public ten::extension_t {
 public:
  explicit test_migration(const char *name) : ten::extension_t(name) {}

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
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "one_event_loop_per_engine": true,
                        "log": {
                          "level": 2
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": true,
                          "singleton": true,
                          "nodes": [{
                            "type": "extension",
                            "name": "migration",
                            "addon": "batch_send_msgs_in_migration__extension",
                            "extension_group": "migration_group"
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

void *app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(batch_send_msgs_in_migration__extension,
                                    test_migration);

}  // namespace

TEST(ExtensionTest, BatchSendMsgsInMigration) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  const int size = 10;

  // Send more than one message asynchronously, the protocol should only
  // transfer one message to the runtime before the connection migration is
  // completed.
  for (int i = 0; i < size; i++) {
    auto test_cmd = ten::cmd_t::create("test");
    test_cmd->set_dest("msgpack://127.0.0.1:8001/", "default",
                       "migration_group", "migration");
    client->send_cmd(std::move(test_cmd));
  }

  int count = 0;
  while (count < size) {
    auto cmd_results = client->batch_recv_cmd_results();

    for (auto &cmd_result : cmd_results) {
      ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
      ten_test::check_detail_with_json(cmd_result, R"({"id":1,"name":"a"})");
      count++;
    }
  }

  delete client;

  ten_thread_join(app_thread, -1);
}
