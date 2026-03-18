//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// Test: sync_stop_before_deinit mode
//
// Topology: extension_a and extension_b in the same graph, each in its own
// extension_group (i.e., separate extension threads).
//
// Invariant under sync_stop_before_deinit:
//   extension_b must NOT enter on_deinit until ALL extensions (including
//   extension_a) have completed on_stop_done.
//
// Test strategy:
//   - extension_a sleeps 2s inside on_stop before calling on_stop_done.
//     Just before calling on_stop_done it sets the global atomic
//     `a_stop_done` to true.
//   - extension_b calls on_stop_done immediately.
//   - extension_b asserts in on_deinit that `a_stop_done` is already true,
//     which would be violated if the runtime allowed b to proceed to
//     on_deinit without waiting for a.
//
#include <atomic>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

// Shared state between extension_a and extension_b.
// Written by extension_a just before on_stop_done; read by extension_b in
// on_deinit.
std::atomic<bool> a_stop_done{
    false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Signals the main test thread that the graph has finished shutting down.
std::atomic<bool> test_completed{
    false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// ─────────────────────────────────────────────────────────────────────────────
// extension_a: slow stopper
// ─────────────────────────────────────────────────────────────────────────────
class test_extension_a : public ten::extension_t {
 public:
  explicit test_extension_a(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_stop(ten::ten_env_t &ten_env) override {
    // Sleep for 2 seconds to ensure extension_b's on_stop_done happens first.
    // In sync mode, extension_b must still wait for us before on_deinit.
    ten_sleep_ms(2000);

    // Mark that we have completed on_stop before notifying the runtime.
    a_stop_done.store(true, std::memory_order_release);

    ten_env.on_stop_done();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// extension_b: fast stopper; validates the sync guarantee in on_deinit
// ─────────────────────────────────────────────────────────────────────────────
class test_extension_b : public ten::extension_t {
 public:
  explicit test_extension_b(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_stop(ten::ten_env_t &ten_env) override {
    // Complete stop immediately — extension_a is still sleeping.
    ten_env.on_stop_done();
  }

  void on_deinit(ten::ten_env_t &ten_env) override {
    // Core assertion: in sync_stop_before_deinit mode, extension_a must have
    // completed on_stop_done before we reach on_deinit.
    ASSERT_TRUE(a_stop_done.load(std::memory_order_acquire))
        << "extension_b entered on_deinit before extension_a completed "
           "on_stop_done — sync_stop_before_deinit barrier was not enforced";

    test_completed.store(true, std::memory_order_release);
    ten_env.on_deinit_done();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────
class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8007/",
               "log": {
                 "handlers": [
                   {
                     "matchers": [{"level": "debug"}],
                     "formatter": {"type": "plain", "colored": true},
                     "emitter": {
                       "type": "console",
                       "config": {"stream": "stdout"}
                     }
                   }
                 ]
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(sync_stop_before_deinit__test_extension_a,
                                    test_extension_a);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(sync_stop_before_deinit__test_extension_b,
                                    test_extension_b);

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Test: SyncStopBeforeDeinit / Basic
//
// Verifies that in sync_stop_before_deinit mode, no extension proceeds to
// on_deinit until all extensions in the graph have completed on_stop_done.
// ─────────────────────────────────────────────────────────────────────────────
TEST(SyncStopBeforeDeinit, Basic) {  // NOLINT
  a_stop_done.store(false, std::memory_order_relaxed);
  test_completed.store(false, std::memory_order_relaxed);

  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8007/");

  // Start the graph with sync_stop_before_deinit enabled.
  // extension_a and extension_b are placed in separate extension_groups so
  // they run on different threads, making the race condition observable if
  // the barrier were absent.
  auto start_graph_cmd = ten::start_graph_cmd_t::create();
  start_graph_cmd->set_graph_from_json(R"({
    "nodes": [
      {
        "type": "extension",
        "name": "test_extension_a",
        "addon": "sync_stop_before_deinit__test_extension_a",
        "extension_group": "group_a",
        "app": "msgpack://127.0.0.1:8007/"
      },
      {
        "type": "extension",
        "name": "test_extension_b",
        "addon": "sync_stop_before_deinit__test_extension_b",
        "extension_group": "group_b",
        "app": "msgpack://127.0.0.1:8007/"
      }
    ]
  })");
  start_graph_cmd->set_sync_stop_before_deinit(true);

  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  delete client;

  ten_thread_join(app_thread, -1);

  ASSERT_TRUE(test_completed.load(std::memory_order_acquire))
      << "Test did not complete — on_deinit of extension_b was never reached";
}
