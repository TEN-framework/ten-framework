//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/value/value_json.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)

namespace {

class creation_extension : public ten::extension_t {
 public:
  creation_extension(const char *name, int64_t started_us)
      : ten::extension_t(name),
        expected_creation_us(ten_current_time_us() - started_us) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    result->set_property("expected_creation_us", expected_creation_us);
    ten_env.return_result(std::move(result));
  }

 private:
  int64_t expected_creation_us;
};

class async_creation_addon : public ten::addon_t {
 public:
  explicit async_creation_addon(ten_runloop_t *loop) : loop_(loop) {}

  ~async_creation_addon() override {
    for (auto &worker : workers_) {
      worker.join();
    }
  }

  void on_create_instance(ten::ten_env_t &ten_env, const char *name,
                          void *context) override {
    int64_t started_us = ten_current_time_us();
    bool slow = std::string(name) == "slow";
    // Make the creation requests overlap with different starting times.
    if (slow) {
      ten_sleep_ms(30);
    }

    struct create_context {
      ten::ten_env_t *ten_env;
      std::string name;
      void *context;
      int64_t started_us;
    };
    auto *ctx = new create_context{&ten_env, name, context, started_us};
    workers_.emplace_back([this, ctx, slow]() {
      ten_sleep_ms(slow ? 90 : 10);
      int rc = ten_runloop_post_task_tail(
          loop_,
          [](void *, void *arg) {
            std::unique_ptr<create_context> ctx(
                static_cast<create_context *>(arg));
            auto *extension = ctx->name == "failed"
                                  ? nullptr
                                  : new creation_extension(ctx->name.c_str(),
                                                           ctx->started_us);
            ctx->ten_env->on_create_instance_done(extension, ctx->context);
          },
          nullptr, ctx);
      TEN_ASSERT(rc == 0, "Failed to complete asynchronous creation.");
    });
  }

  void on_destroy_instance(ten::ten_env_t &ten_env, void *instance,
                           void *context) override {
    delete static_cast<creation_extension *>(instance);
    ten_env.on_destroy_instance_done(context);
  }

 private:
  ten_runloop_t *loop_;
  std::vector<std::thread> workers_;
};

void register_creation_addon(ten_addon_registration_t *,
                             ten_addon_registration_done_func_t done,
                             ten_addon_register_ctx_t *register_ctx,
                             void *user_data) {
  if (!ten_addon_is_registered(register_ctx, TEN_ADDON_TYPE_EXTENSION,
                               "async_creation_duration_test")) {
    auto *addon = new async_creation_addon(
        ten_app_get_attached_runloop(register_ctx->app));
    ten_addon_register_extension(
        "async_creation_duration_test", nullptr,
        static_cast<ten_addon_t *>(addon->get_c_instance()), register_ctx);
  }
  done(register_ctx, user_data);
}

TEN_CONSTRUCTOR(register_async_creation_duration_test) {
  bool rc =
      ten_addon_manager_add_addon(ten_addon_manager_get_instance(), "extension",
                                  "async_creation_duration_test",
                                  register_creation_addon, nullptr, nullptr);
  TEN_ASSERT(rc, "Failed to register creation duration test addon.");
}

void configure_creation_app(ten_app_t *, ten_env_t *ten_env) {
  EXPECT_TRUE(ten_env_init_property_from_json(ten_env, R"({"ten":{
      "uri":"msgpack://127.0.0.1:8001/",
      "services":{"telemetry":{"enabled":true,"metrics":{"enabled":true}}}
    }})",
                                              nullptr));
  ten_env_on_configure_done(ten_env, nullptr);
}

void deinit_creation_app(ten_app_t *app, ten_env_t *ten_env) {
  // All extensions have stopped and deinitialized; the SDK is still alive.
  auto *value = ten_env_peek_property(ten_env, "ten:runtime_stats", nullptr);
  EXPECT_NE(value, nullptr);
  if (value) {
    ten_json_t json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    EXPECT_TRUE(ten_value_to_json(value, &json));
    bool must_free = false;
    const char *text = ten_json_to_string(&json, nullptr, &must_free);
    *static_cast<nlohmann::json *>(app->user_data) =
        nlohmann::json::parse(text);
    if (must_free) {
      TEN_FREE(text);
    }
    ten_json_deinit(&json);
  }
  ten_env_on_deinit_done(ten_env, nullptr);
}

void *creation_app_main(void *arg) {
  auto *app = ten_app_create(configure_creation_app, nullptr,
                             deinit_creation_app, nullptr);
  app->user_data = arg;
  ten_app_run(app, false, nullptr);
  ten_app_destroy(app);
  return nullptr;
}

std::map<std::string, std::map<std::string, double>> lifecycle_durations(
    const nlohmann::json &stats) {
  std::map<std::string, std::map<std::string, double>> durations;
  for (const auto &resource : stats["metrics"]["resourceMetrics"]) {
    for (const auto &scope : resource["scopeMetrics"]) {
      for (const auto &metric : scope["metrics"]) {
        if (metric["name"] != "extension_lifecycle_duration") {
          continue;
        }
        EXPECT_TRUE(metric["gauge"].is_object());
        for (const auto &point : metric["gauge"]["dataPoints"]) {
          std::map<std::string, std::string> attributes;
          for (const auto &attribute : point["attributes"]) {
            attributes[attribute["key"].get<std::string>()] =
                attribute["value"]["stringValue"].get<std::string>();
          }
          durations[attributes["extension_name"]][attributes["stage"]] =
              point["asDouble"].get<double>();
        }
      }
    }
  }
  return durations;
}

}  // namespace

TEST(RuntimeStatsTest, CreationIncludesAsyncWaitForEachInstance) {
  nlohmann::json final_stats;
  std::map<std::string, int64_t> expected_durations;
  auto *thread = ten_thread_create("creation duration app", creation_app_main,
                                   &final_stats);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    auto graph = ten::start_graph_cmd_t::create();
    graph->set_graph_from_json(R"({"nodes":[
      {"type":"extension","name":"slow","addon":"async_creation_duration_test","extension_group":"worker"},
      {"type":"extension","name":"fast","addon":"async_creation_duration_test","extension_group":"worker"}
    ]})");
    auto result = client.send_cmd_and_recv_result(std::move(graph));
    ten_test::check_status_code(result, TEN_STATUS_CODE_OK);
    for (const char *name : {"slow", "fast"}) {
      auto cmd = ten::cmd_t::create("expected_duration");
      cmd->set_dests({{"msgpack://127.0.0.1:8001/", "", name}});
      result = client.send_cmd_and_recv_result(std::move(cmd));
      ten_test::check_status_code(result, TEN_STATUS_CODE_OK);
      expected_durations[name] =
          result->get_property_int64("expected_creation_us");
    }
  }
  ten_thread_join(thread, -1);

  auto durations = lifecycle_durations(final_stats);

  const std::set<std::string> expected_stages = {"creation", "on_configure",
                                                 "on_init",  "on_start",
                                                 "on_stop",  "on_deinit"};
  EXPECT_GE(expected_durations["slow"], 120000);
  EXPECT_GE(expected_durations["fast"], 10000);
  for (const char *name : {"slow", "fast"}) {
    std::set<std::string> stages;
    for (const auto &stage : durations[name]) {
      stages.insert(stage.first);
    }
    EXPECT_EQ(stages, expected_stages) << name;
    if (durations[name].count("creation")) {
      // Allow framework overhead, but exclude waiting for the other instance
      // to finish creating before the extension group starts its lifecycle.
      EXPECT_NEAR(durations[name]["creation"], expected_durations[name], 20000)
          << name;
    }
  }
}

TEST(RuntimeStatsTest, CreationIncludesFailedInstance) {
  nlohmann::json final_stats;
  auto *thread =
      ten_thread_create("failed creation app", creation_app_main, &final_stats);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    auto graph = ten::start_graph_cmd_t::create();
    graph->set_graph_from_json(R"({"nodes":[{
      "type":"extension","name":"failed","addon":"async_creation_duration_test","extension_group":"worker"
    }]})");
    auto result = client.send_cmd_and_recv_result(std::move(graph));
    ten_test::check_status_code(result, TEN_STATUS_CODE_ERROR);
  }
  ten_thread_join(thread, -1);

  auto durations = lifecycle_durations(final_stats);
  ASSERT_EQ(durations.count("failed"), 1);
  EXPECT_EQ(durations["failed"].size(), 1);
  EXPECT_GE(durations["failed"]["creation"], 10000);
}

TEST(RuntimeStatsTest, CreationSkipsUnstartedRequests) {
  nlohmann::json final_stats;
  auto *thread =
      ten_thread_create("missing addon app", creation_app_main, &final_stats);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    auto graph = ten::start_graph_cmd_t::create();
    graph->set_graph_from_json(R"({"nodes":[{
      "type":"extension","name":"failed","addon":"missing_creation_duration_test","extension_group":"worker"
    }]})");
    auto result = client.send_cmd_and_recv_result(std::move(graph));
    ten_test::check_status_code(result, TEN_STATUS_CODE_ERROR);
  }
  ten_thread_join(thread, -1);

  EXPECT_TRUE(lifecycle_durations(final_stats).empty());
}

#endif
