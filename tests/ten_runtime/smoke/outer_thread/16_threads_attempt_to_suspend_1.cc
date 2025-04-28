//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/macro/check.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#define TEST_DATA_VALUE 0x34CE87AB478D2DBE

namespace {

class Holder {  // NOLINT
 public:
  explicit Holder(void *ptr) : ptr_(ptr) {}
  ~Holder() { TEN_FREE(ptr_); }

 private:
  void *ptr_{nullptr};
};

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

#define OUTER_THREAD_MAIN(X)                                                 \
  void outer_thread##X##_main(                                               \
      ten::ten_env_proxy_t *ten_env_proxy) { /* NOLINT */                    \
    auto *test_data = static_cast<int64_t *>(ten_malloc(sizeof(int64_t)));   \
    TEN_ASSERT(test_data, "Failed to allocate memory.");                     \
                                                                             \
    Holder _(test_data);                                                     \
                                                                             \
    *test_data = TEST_DATA_VALUE;                                            \
                                                                             \
    std::unique_lock<std::mutex> lock(outer_thread_##X##_cv_lock);           \
                                                                             \
    for (int i = 0; i < 10; ++i) {                                           \
      if (!outer_thread_##X##_towards_to_close) {                            \
        bool rc = ten_env_proxy->acquire_lock_mode();                        \
        TEN_ASSERT(rc, "Failed to acquire lock mode.");                      \
                                                                             \
        ten_env_proxy->notify(send_data_from_outer_thread, test_data, true); \
                                                                             \
        rc = ten_env_proxy->release_lock_mode();                             \
        TEN_ASSERT(rc, "Failed to release lock mode.");                      \
      }                                                                      \
    }                                                                        \
    delete ten_env_proxy;                                                    \
    while (!outer_thread_##X##_towards_to_close) {                           \
      outer_thread_##X##_cv.wait(lock);                                      \
    }                                                                        \
  }

  OUTER_THREAD_MAIN(1)
  OUTER_THREAD_MAIN(2)
  OUTER_THREAD_MAIN(3)
  OUTER_THREAD_MAIN(4)
  OUTER_THREAD_MAIN(5)
  OUTER_THREAD_MAIN(6)
  OUTER_THREAD_MAIN(7)
  OUTER_THREAD_MAIN(8)
  OUTER_THREAD_MAIN(9)
  OUTER_THREAD_MAIN(10)
  OUTER_THREAD_MAIN(11)
  OUTER_THREAD_MAIN(12)
  OUTER_THREAD_MAIN(13)
  OUTER_THREAD_MAIN(14)
  OUTER_THREAD_MAIN(15)
  OUTER_THREAD_MAIN(16)

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_to_send_cmd = ten::cmd_t::create("start_to_send");
    ten_env.send_cmd(std::move(start_to_send_cmd),
                     [this](ten::ten_env_t &ten_env,
                            std::unique_ptr<ten::cmd_result_t> cmd_result,
                            ten::error_t *err) {
                       TEN_ASSERT(
                           cmd_result->get_status_code() == TEN_STATUS_CODE_OK,
                           "Failed to send 'start_to_send' command.");

#define CREATE_OUTER_THREAD(X)                                           \
  do {                                                                   \
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);         \
    outer_thread##X = new std::thread(                                   \
        &test_extension_1::outer_thread##X##_main, this, ten_env_proxy); \
  } while (0)

                       CREATE_OUTER_THREAD(1);
                       CREATE_OUTER_THREAD(2);
                       CREATE_OUTER_THREAD(3);
                       CREATE_OUTER_THREAD(4);
                       CREATE_OUTER_THREAD(5);
                       CREATE_OUTER_THREAD(6);
                       CREATE_OUTER_THREAD(7);
                       CREATE_OUTER_THREAD(8);
                       CREATE_OUTER_THREAD(9);
                       CREATE_OUTER_THREAD(10);
                       CREATE_OUTER_THREAD(11);
                       CREATE_OUTER_THREAD(12);
                       CREATE_OUTER_THREAD(13);
                       CREATE_OUTER_THREAD(14);
                       CREATE_OUTER_THREAD(15);
                       CREATE_OUTER_THREAD(16);

                       return true;
                     });

    ten_env.on_start_done();
  }

  void on_stop(ten::ten_env_t &ten_env) override {
#define NOTIFY_OUTER_THREAD_TO_STOP(X)                               \
  do {                                                               \
    {                                                                \
      std::unique_lock<std::mutex> lock(outer_thread_##X##_cv_lock); \
      outer_thread_##X##_towards_to_close = true;                    \
    }                                                                \
    outer_thread_##X##_cv.notify_one();                              \
  } while (0)

    NOTIFY_OUTER_THREAD_TO_STOP(1);
    NOTIFY_OUTER_THREAD_TO_STOP(2);
    NOTIFY_OUTER_THREAD_TO_STOP(3);
    NOTIFY_OUTER_THREAD_TO_STOP(4);
    NOTIFY_OUTER_THREAD_TO_STOP(5);
    NOTIFY_OUTER_THREAD_TO_STOP(6);
    NOTIFY_OUTER_THREAD_TO_STOP(7);
    NOTIFY_OUTER_THREAD_TO_STOP(8);
    NOTIFY_OUTER_THREAD_TO_STOP(9);
    NOTIFY_OUTER_THREAD_TO_STOP(10);
    NOTIFY_OUTER_THREAD_TO_STOP(11);
    NOTIFY_OUTER_THREAD_TO_STOP(12);
    NOTIFY_OUTER_THREAD_TO_STOP(13);
    NOTIFY_OUTER_THREAD_TO_STOP(14);
    NOTIFY_OUTER_THREAD_TO_STOP(15);
    NOTIFY_OUTER_THREAD_TO_STOP(16);

#define RECLAIM_OUTER_THREAD(X) \
  do {                          \
    outer_thread##X->join();    \
    delete outer_thread##X;     \
  } while (0)

    RECLAIM_OUTER_THREAD(1);
    RECLAIM_OUTER_THREAD(2);
    RECLAIM_OUTER_THREAD(3);
    RECLAIM_OUTER_THREAD(4);
    RECLAIM_OUTER_THREAD(5);
    RECLAIM_OUTER_THREAD(6);
    RECLAIM_OUTER_THREAD(7);
    RECLAIM_OUTER_THREAD(8);
    RECLAIM_OUTER_THREAD(9);
    RECLAIM_OUTER_THREAD(10);
    RECLAIM_OUTER_THREAD(11);
    RECLAIM_OUTER_THREAD(12);
    RECLAIM_OUTER_THREAD(13);
    RECLAIM_OUTER_THREAD(14);
    RECLAIM_OUTER_THREAD(15);
    RECLAIM_OUTER_THREAD(16);

    ten_env.on_stop_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {}

 private:
#define OTHER_THREAD_CB_PACK(X)                  \
  std::thread *outer_thread##X{nullptr};         \
  std::mutex outer_thread_##X##_cv_lock;         \
  std::condition_variable outer_thread_##X##_cv; \
  bool outer_thread_##X##_towards_to_close{false};

  OTHER_THREAD_CB_PACK(1)
  OTHER_THREAD_CB_PACK(2)
  OTHER_THREAD_CB_PACK(3)
  OTHER_THREAD_CB_PACK(4)
  OTHER_THREAD_CB_PACK(5)
  OTHER_THREAD_CB_PACK(6)
  OTHER_THREAD_CB_PACK(7)
  OTHER_THREAD_CB_PACK(8)
  OTHER_THREAD_CB_PACK(9)
  OTHER_THREAD_CB_PACK(10)
  OTHER_THREAD_CB_PACK(11)
  OTHER_THREAD_CB_PACK(12)
  OTHER_THREAD_CB_PACK(13)
  OTHER_THREAD_CB_PACK(14)
  OTHER_THREAD_CB_PACK(15)
  OTHER_THREAD_CB_PACK(16)

  static void send_data_from_outer_thread(ten::ten_env_t &ten_env,
                                          void *user_data) {
    // Create a 'ten::data_t' with the same important data.
    auto ten_data = ten::data_t::create("data");
    ten_data->set_property("test_data", user_data);
    ten_env.send_data(std::move(ten_data));
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == std::string("start_to_send")) {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "ok");
      ten_env.return_result(std::move(cmd_result));
      return;
    }

    if (data_received_count == expected_received_count) {
      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("detail", "ok");
      ten_env.return_result(std::move(cmd_result));
    } else {
      hello_cmd = std::move(cmd);
    }
  }

  void on_data(TEN_UNUSED ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    auto *test_data =
        static_cast<int64_t *>(data->get_property_ptr("test_data"));
    TEN_ASSERT(*test_data == TEST_DATA_VALUE, "test_data has been destroyed.");

    data_received_count++;

    if (hello_cmd != nullptr &&
        data_received_count == expected_received_count) {
      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *hello_cmd);
      cmd_result->set_property("detail", "ok");
      ten_env.return_result(std::move(cmd_result));
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> hello_cmd{nullptr};
  int data_received_count{0};
  int expected_received_count{160};
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    sixteen_threads_attempt_to_suspend_1__test_extension_1, test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    sixteen_threads_attempt_to_suspend_1__test_extension_2, test_extension_2);

}  // namespace

TEST(OuterThreadTest, SixteenThreadsAttemptToSuspend1) {  // NOLINT
  // Start app.
  auto *app_thread =
      ten_thread_create("app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send graph.
  auto start_graph_cmd = ten::cmd_start_graph_t::create();
  start_graph_cmd->set_graph_from_json(R"({
           "nodes": [{
               "type": "extension",
               "name": "test_extension_1",
               "addon": "sixteen_threads_attempt_to_suspend_1__test_extension_1",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "basic_extension_group"
             },{
               "type": "extension",
               "name": "test_extension_2",
               "addon": "sixteen_threads_attempt_to_suspend_1__test_extension_2",
               "app": "msgpack://127.0.0.1:8001/",
               "extension_group": "basic_extension_group"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "test_extension_1",
               "cmd": [{
                 "name": "start_to_send",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }],
               "data": [{
                 "name": "data",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }]
             }]
           })");
  auto cmd_result =
      client->send_cmd_and_recv_result(std::move(start_graph_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);

  // Send a user-defined 'hello world' command.
  auto hello_world_cmd = ten::cmd_t::create("hello_world");
  hello_world_cmd->set_dest("msgpack://127.0.0.1:8001/", nullptr,
                            "basic_extension_group", "test_extension_2");
  cmd_result = client->send_cmd_and_recv_result(std::move(hello_world_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "ok");

  delete client;

  ten_thread_join(app_thread, -1);
}
