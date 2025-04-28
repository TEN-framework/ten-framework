//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/extension.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

// This part is the extension codes written by the developer, maintained in its
// final release form, and will not change due to testing requirements.

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    if (std::string(data->get_name()) == "test") {
      data_frame_recv_count_++;

      return_if_possible(ten_env);
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

  void on_audio_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::audio_frame_t> audio_frame) override {
    if (std::string(audio_frame->get_name()) == "test") {
      data_frame_recv_count_++;

      return_if_possible(ten_env);
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

  void on_video_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::video_frame_t> video_frame) override {
    if (std::string(video_frame->get_name()) == "test") {
      data_frame_recv_count_++;

      return_if_possible(ten_env);
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      // Cache the cmd and return it later.
      cached_cmd_ = std::move(cmd);

      return_if_possible(ten_env);
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

  void return_if_possible(ten::ten_env_t &ten_env) {
    if (cached_cmd_ && data_frame_recv_count_ == 3) {
      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cached_cmd_);
      cmd_result->set_property("detail", "hello world, too");
      bool rc = ten_env.return_result(std::move(cmd_result));
      EXPECT_EQ(rc, true);
    }
  }

 private:
  int data_frame_recv_count_ = 0;
  std::unique_ptr<ten::cmd_t> cached_cmd_;
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    standalone_test_basic_data__test_extension_1, test_extension_1);

}  // namespace

namespace {

class extension_tester_1 : public ten::extension_tester_t {
 public:
  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Send the first command to the extension.
    auto new_cmd = ten::cmd_t::create("hello_world");

    ten_env.send_cmd(
        std::move(new_cmd),
        [](ten::ten_env_tester_t &ten_env,
           std::unique_ptr<ten::cmd_result_t> cmd_result, ten::error_t *err) {
          if (cmd_result->get_status_code() == TEN_STATUS_CODE_OK) {
            ten_env.stop_test();
          }
        });

    ten_env.send_data(ten::data_t::create("test"));
    ten_env.send_audio_frame(ten::audio_frame_t::create("test"));
    ten_env.send_video_frame(ten::video_frame_t::create("test"));

    ten_env.on_start_done();
  }
};

}  // namespace

TEST(StandaloneTest, BasicData) {  // NOLINT
  auto *tester = new extension_tester_1();
  tester->set_test_mode_single("standalone_test_basic_data__test_extension_1");

  bool rc = tester->run();
  TEN_ASSERT(rc, "Should not happen.");

  delete tester;
}
