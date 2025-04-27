//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension : public ten::extension_t {
 public:
  explicit test_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    TEN_ASSERT(0, "Should not happen.");
  }
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8001/",
               "log": {
                 "level": 2
               }
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

class graph_failed_to_create_extension__test_extension_default_extension_addon_t
    : public ten::addon_t {
 public:
  void on_create_instance(ten::ten_env_t &ten_env, const char *name,
                          void *context) override {
    // This line is important. The first parameter is `nullptr`, used to
    // simulate the failure of creating an extension.
    ten_env.on_create_instance_done(nullptr, context);
  }
  void on_destroy_instance(ten::ten_env_t &ten_env, void *instance,
                           void *context) override {
    TEN_ASSERT(0, "Should not happen.");
  }
};

void ____ten_addon_graph_failed_to_create_extension__test_extension_register_handler__(
    TEN_UNUSED TEN_ADDON_TYPE addon_type, TEN_UNUSED ten_string_t *addon_name,
    void *register_ctx, TEN_UNUSED void *user_data) {
  auto *addon_instance =
      new graph_failed_to_create_extension__test_extension_default_extension_addon_t();
  ten_string_t *base_dir =
      ten_path_get_module_path(/* NOLINTNEXTLINE */
                               (void *)
                                   ____ten_addon_graph_failed_to_create_extension__test_extension_register_handler__);
  ten_addon_register_extension(
      "graph_failed_to_create_extension__test_extension",
      ten_string_get_raw_str(base_dir),
      static_cast<ten_addon_t *>(addon_instance->get_c_instance()),
      register_ctx);
  ten_string_destroy(base_dir);
}

TEN_CONSTRUCTOR(
    ____ten_addon_graph_failed_to_create_extension__test_extension_registrar____) {
  /* Add addon registration function into addon manager. */
  ten_addon_manager_t *manager = ten_addon_manager_get_instance();
  bool success = ten_addon_manager_add_addon(
      manager, "extension", "graph_failed_to_create_extension__test_extension",
      ____ten_addon_graph_failed_to_create_extension__test_extension_register_handler__,
      nullptr, nullptr);
  if (!success) {
    TEN_LOGF("Failed to register addon: %s",
             "graph_failed_to_create_extension__test_extension");
    /* NOLINTNEXTLINE(concurrency-mt-unsafe) */
    exit(EXIT_FAILURE);
  }
}

}  // namespace

TEST(GraphErrorTest, FailedToCreateExtension) {  // NOLINT
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
                "type": "extension",
                "name": "test_extension",
                "addon": "graph_failed_to_create_extension__test_extension",
                "extension_group": "test_extension_group",
                "app": "msgpack://127.0.0.1:8001/"
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_ERROR);

  delete client;

  ten_thread_join(app_thread, -1);
}
