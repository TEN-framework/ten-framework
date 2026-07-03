//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <condition_variable>
#include <thread>
#include <utility>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/extension.h"
#include "ten_runtime/binding/cpp/detail/ten_env_proxy.h"
#include "ten_runtime/binding/cpp/detail/test/env_tester.h"
#include "ten_runtime/binding/cpp/detail/test/env_tester_proxy.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/macro/check.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

// This part is the extension codes written by the developer, maintained in its
// final release form, and will not change due to testing requirements.

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "process") {
      auto data = cmd->get_property_int64("data");
      cmd->set_property("data", data * 2);

      ten_env.send_cmd(std::move(cmd));
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }
};

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "process") {
      auto data = cmd->get_property_int64("data");

      auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
      cmd_result->set_property("data", data * data);

      ten_env.return_result(std::move(cmd_result));

      // Send another command after 1 second.
      auto *ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);
      greeting_thread_ = std::thread([ten_env_proxy]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        ten_env_proxy->notify([](ten::ten_env_t &ten_env) {
          auto new_cmd = ten::cmd_t::create("hello_world");
          ten_env.send_cmd(std::move(new_cmd));
        });

        delete ten_env_proxy;
      });
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

  void on_stop(ten::ten_env_t &ten_env) override {
    if (greeting_thread_.joinable()) {
      greeting_thread_.join();
    }

    ten_env.on_stop_done();
  }

 private:
  std::thread greeting_thread_;
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    standalone_test_basic_graph_outer_thread_2__test_extension_1,
    test_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    standalone_test_basic_graph_outer_thread_2__test_extension_2,
    test_extension_2);

}  // namespace

namespace {

class extension_tester_1 : public ten::extension_tester_t {
 public:
  void set_on_started_callback(
      std::function<void(ten::ten_env_tester_t &ten_env)> callback) {
    on_started_callback_ = std::move(callback);
  }

  void set_on_hello_world_callback(
      std::function<void(ten::ten_env_tester_t &ten_env,
                         std::unique_ptr<ten::cmd_t> cmd)>
          callback) {
    on_hello_world_callback_ = std::move(callback);
  }

 protected:
  void on_start(ten::ten_env_tester_t &ten_env) override {
    ten_env.on_start_done();

    if (on_started_callback_) {
      on_started_callback_(ten_env);
    }
  }

  void on_cmd(ten::ten_env_tester_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "hello_world") {
      if (on_hello_world_callback_) {
        on_hello_world_callback_(ten_env, std::move(cmd));
      }
    }
  }

 private:
  std::function<void(ten::ten_env_tester_t &ten_env)> on_started_callback_;
  std::function<void(ten::ten_env_tester_t &ten_env,
                     std::unique_ptr<ten::cmd_t> cmd)>
      on_hello_world_callback_;
};

}  // namespace

typedef struct tester_context_t {
  std::mutex mtx;
  std::condition_variable cv;
  ten::ten_env_tester_proxy_t *ten_env_proxy{nullptr};
} tester_context_t;

TEST(StandaloneTest, BasicGraphOuterThread2) {  // NOLINT
  tester_context_t tester_context;

  std::thread tester_thread([&tester_context]() {
    auto *tester = new extension_tester_1();

    // The graph is like:
    //
    // ten:test_extension -> test_extension_1 -> test_extension_2
    //        ^                                        |
    //        |                                        v
    //         ----------------------------------------
    //
    tester->set_test_mode_graph(R"({
    "nodes": [{
			"type": "extension",
			"name": "test_extension_1",
			"addon": "standalone_test_basic_graph_outer_thread_2__test_extension_1",
			"extension_group": "test_extension_group_1"
		},
		{
			"type": "extension",
			"name": "test_extension_2",
			"addon": "standalone_test_basic_graph_outer_thread_2__test_extension_2",
			"extension_group": "test_extension_group_2"
		},
		{
			"type": "extension",
			"name": "ten:test_extension",
			"addon": "ten:test_extension",
			"extension_group": "test_extension_group"
		}],
		"connections": [{
			"extension": "ten:test_extension",
			"cmd": [{
				"name": "process",
				"dest": [{
					"extension": "test_extension_1"
				}]
			}]
		},
		{
			"extension": "test_extension_1",
			"cmd": [{
				"name": "process",
				"dest": [{
					"extension": "test_extension_2"
				}]
			}]
		},
		{
			"extension": "test_extension_2",
			"cmd": [{
				"name": "hello_world",
				"dest": [{
					"extension": "ten:test_extension"
				}]
			}]
		}]})");

    tester->set_on_started_callback(
        [&tester_context](ten::ten_env_tester_t &ten_env) {
          std::lock_guard<std::mutex> lock(tester_context.mtx);
          tester_context.ten_env_proxy =
              ten::ten_env_tester_proxy_t::create(ten_env);
          tester_context.cv.notify_all();
        });

    tester->set_on_hello_world_callback(
        [&tester_context](ten::ten_env_tester_t &ten_env,
                          std::unique_ptr<ten::cmd_t> /*cmd*/) {
          delete tester_context.ten_env_proxy;
          ten_env.stop_test();
        });

    bool rc = tester->run();
    TEN_ASSERT(rc, "Should not happen.");

    delete tester;
  });

  std::unique_lock<std::mutex> lock(tester_context.mtx);
  tester_context.cv.wait(lock, [&tester_context]() {
    return tester_context.ten_env_proxy != nullptr;
  });

  // Send command to the graph in the role of 'ten:test_extension' and check
  // the returned result.
  tester_context.ten_env_proxy->notify(
      [](ten::ten_env_tester_t &ten_env) {
        auto process_cmd = ten::cmd_t::create("process");
        process_cmd->set_property("data", 3);

        ten_env.send_cmd(
            std::move(process_cmd),
            [](ten::ten_env_tester_t & /*ten_env*/,
               std::unique_ptr<ten::cmd_result_t> result, ten::error_t *err) {
              auto data = result->get_property_int64("data");
              EXPECT_EQ(data, 36);
            });
      },
      nullptr);

  tester_thread.join();
}
