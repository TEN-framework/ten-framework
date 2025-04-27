//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>
#include <thread>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/ten_env_proxy.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    auto cmd = ten::cmd_t::create("test");
    ten_env.send_cmd(std::move(cmd));

    auto data = ten::data_t::create("test");
    ten_env.send_data(std::move(data));

    auto audio_frame = ten::audio_frame_t::create("test");
    ten_env.send_audio_frame(std::move(audio_frame));

    auto video_frame = ten::video_frame_t::create("test");
    ten_env.send_video_frame(std::move(video_frame));

    ten_env.on_start_done();
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_init(ten::ten_env_t &ten_env) override {
    auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    start_thread_ = std::thread([ten_env_proxy, this]() {
      ten_sleep_ms(1000);

      ten_env_proxy->notify([this](ten::ten_env_t &ten_env) {
        // Only after calling on_init_done(), commands can be processed through
        // the on_cmd callback.

        // Record the timestamp of on_init_done()
        init_done_time_ms_ = ten_current_time_ms();

        ten_env.on_init_done();
      });

      delete ten_env_proxy;
    });
  }

  void close_app_if_all_msg_received(ten::ten_env_t &ten_env) const {
    if (msg_received_count_ == 4) {
      auto close_app_cmd = ten::cmd_close_app_t::create();
      close_app_cmd->set_dest("localhost", nullptr, nullptr, nullptr);
      ten_env.send_cmd(std::move(close_app_cmd));
    }
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    ASSERT_EQ(cmd->get_name(), "test");

    auto current_time = ten_current_time_ms();
    // current_time should be greater than init_done_time_ms_.
    ASSERT_GE(current_time, init_done_time_ms_);

    msg_received_count_++;

    close_app_if_all_msg_received(ten_env);
  }

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    ASSERT_EQ(data->get_name(), "test");
    auto current_time = ten_current_time_ms();
    // current_time should not be less than init_done_time_ms_
    ASSERT_GE(current_time, init_done_time_ms_);

    msg_received_count_++;

    close_app_if_all_msg_received(ten_env);
  }

  void on_audio_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::audio_frame_t> audio_frame) override {
    ASSERT_EQ(audio_frame->get_name(), "test");
    auto current_time = ten_current_time_ms();
    // current_time should not be less than init_done_time_ms_
    ASSERT_GE(current_time, init_done_time_ms_);

    msg_received_count_++;

    close_app_if_all_msg_received(ten_env);
  }

  void on_video_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::video_frame_t> video_frame) override {
    ASSERT_EQ(video_frame->get_name(), "test");
    auto current_time = ten_current_time_ms();
    // current_time should not be less than init_done_time_ms_
    ASSERT_GE(current_time, init_done_time_ms_);

    msg_received_count_++;

    close_app_if_all_msg_received(ten_env);
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    ASSERT_EQ(msg_received_count_, 4);

    ten_env.on_stop_done();
  }

  void on_deinit(ten::ten_env_t &ten_env) override {
    if (start_thread_.joinable()) {
      start_thread_.join();
    }

    ten_env.on_deinit_done();
  }

 private:
  std::thread start_thread_;
  int64_t init_done_time_ms_ = INT64_MAX;

  int msg_received_count_ = 0;
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(recv_msg_defore_inited__test_extension_1,
                                    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(recv_msg_defore_inited__test_extension_2,
                                    test_extension_2);

}  // namespace

// This test is to verify that the extension cannot receive
// msgs(cmd/data/audio/video) before on_init_done is called.
TEST(TenEnvCallTimingTest, RecvMsgBeforeInited) {  // NOLINT
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
                "addon": "recv_msg_defore_inited__test_extension_1",
                "extension_group": "basic_extension_group_1",
                "app": "msgpack://127.0.0.1:8001/"
             },{
                "type": "extension",
                "name": "test_extension_2",
                "addon": "recv_msg_defore_inited__test_extension_2",
                "extension_group": "basic_extension_group_2",
                "app": "msgpack://127.0.0.1:8001/"
             }],
             "connections": [{
               "app": "msgpack://127.0.0.1:8001/",
               "extension": "test_extension_1",
               "cmd": [{
                 "name": "test",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }],
               "data": [{
                 "name": "test",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }],
               "audio_frame": [{
                 "name": "test",
                 "dest": [{
                   "app": "msgpack://127.0.0.1:8001/",
                   "extension": "test_extension_2"
                 }]
               }],
               "video_frame": [{
                 "name": "test",
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

  // Wait for the app auto close.

  ten_thread_join(app_thread, -1);

  delete client;
}
