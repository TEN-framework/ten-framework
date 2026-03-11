//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// Test: SyncStopBeforeDeinit / MsgDuringOnStop
//
// Topology
// ────────
//   predefined "default" graph
//     test_extension_1  — orchestrator; starts/stops the dynamic graph and
//                         collects the notifications sent during on_stop.
//
//   dynamic graph  (sync_stop_before_deinit = true)
//     test_extension_2  — on_stop: sleep 2s → send on_stop_notify → done
//     test_extension_3  — on_stop: immediately send on_stop_notify → done
//
// Both dynamic extensions expose on_stop_notify as cmd_out so that it is
// routed out of the dynamic graph and received by test_extension_1.
//
// What we are testing
// ───────────────────
// 1. Messages sent during on_stop are delivered to extensions outside the
//    graph, even in sync_stop_before_deinit mode.
// 2. The 2-second sleep makes it observable that the message is genuinely
//    sent while the graph is in its stop phase, not after cleanup.
// 3. stop_graph is only completed after both extensions finish on_deinit
//    (which, in sync mode, happens after BOTH on_stop_done calls), so
//    test_extension_1 always receives the two notifications before (or at
//    the same time as) the stop_graph result.
//
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/stop_graph_cmd.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// test_extension_1  (predefined "default" graph)
// ─────────────────────────────────────────────────────────────────────────────
class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      // Save the client cmd; reply only when all expected events arrive.
      test_cmd_ = std::move(cmd);

      // Start the dynamic graph with sync_stop_before_deinit enabled.
      auto start_cmd = ten::start_graph_cmd_t::create();
      start_cmd->set_dests({{""}});
      start_cmd->set_sync_stop_before_deinit(true);
      start_cmd->set_graph_from_json(R"({
        "nodes": [
          {
            "type": "extension",
            "name": "test_extension_2",
            "addon": "sync_stop_msg_during_stop__test_extension_2",
            "app": "msgpack://127.0.0.1:8008/",
            "extension_group": "group_2"
          },
          {
            "type": "extension",
            "name": "test_extension_3",
            "addon": "sync_stop_msg_during_stop__test_extension_3",
            "app": "msgpack://127.0.0.1:8008/",
            "extension_group": "group_3"
          }
        ],
        "exposed_messages": [
          {
            "type": "cmd_out",
            "name": "on_stop_notify",
            "extension": "test_extension_2"
          },
          {
            "type": "cmd_out",
            "name": "on_stop_notify",
            "extension": "test_extension_3"
          }
        ]
      })");

      ten_env.send_cmd(
          std::move(start_cmd),
          [this](ten::ten_env_t &ten_env,
                 std::unique_ptr<ten::cmd_result_t> cmd_result,
                 ten::error_t * /*err*/) {
            ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

            auto graph_id = cmd_result->get_property_string("graph_id");
            TEN_LOGI("Dynamic graph started, graph_id=%s", graph_id.c_str());

            // Stop the graph immediately; this triggers on_stop for both
            // dynamic extensions.
            auto stop_cmd = ten::stop_graph_cmd_t::create();
            stop_cmd->set_dests({{""}});
            stop_cmd->set_graph_id(graph_id.c_str());

            ten_env.send_cmd(
                std::move(stop_cmd),
                [this](ten::ten_env_t &ten_env,
                       std::unique_ptr<ten::cmd_result_t> /*cmd_result*/,
                       ten::error_t * /*err*/) {
                  TEN_LOGI(
                      "stop_graph done, notify_count=%d, stop_graph_done=%d",
                      notify_count_, stop_graph_done_);

                  stop_graph_done_ = true;
                  try_reply(ten_env);
                });
          });

    } else if (cmd->get_name() == "on_stop_notify") {
      auto sender = cmd->get_property_string("sender");
      TEN_LOGI("Received on_stop_notify from extension_%s", sender.c_str());

      // Must reply so the sending extension's send_cmd callback fires and it
      // can call on_stop_done.
      ten_env.return_result(
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd));

      notify_count_++;
      try_reply(ten_env);

    } else {
      TEN_ASSERT(0, "Unexpected cmd: %s", cmd->get_name().c_str());
    }
  }

 private:
  void try_reply(ten::ten_env_t &ten_env) {
    if (stop_graph_done_ && notify_count_ >= 2 && test_cmd_ != nullptr) {
      TEN_LOGI("All conditions met, replying to client");

      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *test_cmd_);
      cmd_result->set_property("notify_count", notify_count_);
      ten_env.return_result(std::move(cmd_result));
      test_cmd_.reset();
    }
  }

  std::unique_ptr<ten::cmd_t> test_cmd_;
  int notify_count_{0};
  bool stop_graph_done_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// test_extension_2  (dynamic graph, slow stopper)
// Sleeps 2s in on_stop to simulate heavy cleanup, then sends on_stop_notify
// before calling on_stop_done.
// ─────────────────────────────────────────────────────────────────────────────
class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_stop(ten::ten_env_t &ten_env) override {
    // Simulate a 2-second cleanup task.
    ten_sleep_ms(2000);

    TEN_LOGI("[test_extension_2] on_stop: sending on_stop_notify after 2s");

    auto notify = ten::cmd_t::create("on_stop_notify");
    notify->set_property("sender", "2");

    // Call on_stop_done only after the external extension acknowledges the
    // notification — this validates that the reply reaches us during on_stop.
    ten_env.send_cmd(
        std::move(notify), [](ten::ten_env_t &ten_env,
                              std::unique_ptr<ten::cmd_result_t> cmd_result,
                              ten::error_t * /*err*/) {
          ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
          TEN_LOGI("[test_extension_2] on_stop_notify ack received");
          ten_env.on_stop_done();
        });
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// test_extension_3  (dynamic graph, fast stopper)
// Immediately sends on_stop_notify and then calls on_stop_done.
// In sync_stop_before_deinit mode this extension must wait for extension_2's
// on_stop_done before it can proceed to on_deinit.
// ─────────────────────────────────────────────────────────────────────────────
class test_extension_3 : public ten::extension_t {
 public:
  explicit test_extension_3(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override { ten_env.on_start_done(); }

  void on_stop(ten::ten_env_t &ten_env) override {
    TEN_LOGI("[test_extension_3] on_stop: sending on_stop_notify immediately");

    auto notify = ten::cmd_t::create("on_stop_notify");
    notify->set_property("sender", "3");

    ten_env.send_cmd(
        std::move(notify), [](ten::ten_env_t &ten_env,
                              std::unique_ptr<ten::cmd_result_t> cmd_result,
                              ten::error_t * /*err*/) {
          ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
          TEN_LOGI("[test_extension_3] on_stop_notify ack received");
          ten_env.on_stop_done();
        });
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────
class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env, R"({"type": "app", "name": "test_app", "version": "0.1.0"})");
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        // clang-format off
        R"({
             "ten": {
               "uri": "msgpack://127.0.0.1:8008/",
               "log": {
                 "handlers": [{
                   "matchers": [{"level": "debug"}],
                   "formatter": {"type": "plain", "colored": true},
                   "emitter": {"type": "console", "config": {"stream": "stdout"}}
                 }]
               },
               "predefined_graphs": [{
                 "name": "default",
                 "auto_start": false,
                 "singleton": true,
                 "graph": {
                   "nodes": [{
                     "type": "extension",
                     "name": "test_extension_1",
                     "addon": "sync_stop_msg_during_stop__test_extension_1",
                     "extension_group": "default_group"
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

void *app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;
  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(sync_stop_msg_during_stop__test_extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(sync_stop_msg_during_stop__test_extension_2,
                                    test_extension_2);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(sync_stop_msg_during_stop__test_extension_3,
                                    test_extension_3);

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Test: SyncStopBeforeDeinit / MsgDuringOnStop
//
// Verifies that messages sent from dynamic-graph extensions during on_stop
// are correctly delivered to an external extension, even with
// sync_stop_before_deinit enabled and a 2-second sleep in one extension's
// on_stop. The stop_graph command is only completed after all on_deinit_done
// calls, which in sync mode happens after BOTH on_stop_done calls —
// guaranteeing both notifications are received before the test finishes.
// ─────────────────────────────────────────────────────────────────────────────
TEST(SyncStopBeforeDeinit, MsgDuringOnStop) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8008/");

  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8008/", "default", "test_extension_1"}});

  // send_cmd_and_recv_result blocks until test_extension_1 replies.
  // test_extension_1 replies only after:
  //   - both on_stop_notify messages are received (count == 2), AND
  //   - stop_graph has completed.
  // With sync_stop_before_deinit, stop_graph completes only after both
  // extensions have called on_stop_done, which happens after their
  // on_stop_notify acknowledgements arrive — so by the time we return here,
  // both messages have been delivered.
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Confirm exactly 2 notifications were received.
  ASSERT_EQ(cmd_result->get_property_int32("notify_count"), 2);

  delete client;
  ten_thread_join(app_thread, -1);
}
