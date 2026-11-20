//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <atomic>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/io/runloop.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/value/value_json.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

#if defined(TEN_ENABLE_TEN_RUST_APIS)

namespace {

constexpr const char *startup_addon_name = "graph_startup_duration_test";

struct startup_state {
  bool own_engine_loop = false;
  bool auto_start = false;
  bool reverse_creation_order = false;
  ten_event_t *configure_entered = ten_event_create(0, 0);
  ten_event_t *release_configure = ten_event_create(0, 0);
  std::atomic<int64_t> latest_configure_started_us{0};
  std::atomic<bool> configure_done{false};
  nlohmann::json final_stats;

  ~startup_state() {
    ten_event_destroy(configure_entered);
    ten_event_destroy(release_configure);
  }
};

std::atomic<startup_state *> active_startup_state{nullptr};

nlohmann::json startup_graph() {
  return {{"nodes",
           {
               {{"type", "extension"},
                {"name", "slow"},
                {"addon", startup_addon_name},
                {"extension_group", "slow_group"}},
               {{"type", "extension"},
                {"name", "fast"},
                {"addon", startup_addon_name},
                {"extension_group", "fast_group"}},
               {{"type", "extension"},
                {"name", "observer"},
                {"addon", startup_addon_name},
                {"extension_group", "fast_group"}},
           }}};
}

class startup_extension : public ten::extension_t {
 public:
  startup_extension(const char *name, startup_state *state)
      : ten::extension_t(name),
        slow_(std::string(name) == "slow"),
        state_(state) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    int64_t started_us = ten_current_time_us();
    int64_t previous_us = state_->latest_configure_started_us.load();
    while (previous_us < started_us &&
           !state_->latest_configure_started_us.compare_exchange_weak(
               previous_us, started_us)) {
    }
    if (slow_) {
      ten_event_set(state_->configure_entered);
      ten_event_wait(state_->release_configure, -1);
      state_->configure_done = true;
    }
    ten_env.on_configure_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    result->set_property("detail",
                         ten_env.get_property_to_json("ten:runtime_stats"));
    ten_env.return_result(std::move(result));
  }

 private:
  bool slow_;
  startup_state *state_;
};

class startup_addon : public ten::addon_t {
 public:
  explicit startup_addon(ten_runloop_t *loop) : loop_(loop) {}

  ~startup_addon() override {
    for (auto &worker : workers_) {
      worker.join();
    }
  }

  void on_create_instance(ten::ten_env_t &ten_env, const char *name,
                          void *context) override {
    struct completion {
      ten::ten_env_t *ten_env;
      std::string name;
      void *context;
      startup_state *state;
    };
    auto *data =
        new completion{&ten_env, name, context, active_startup_state.load()};
    int delay_ms = data->name == "slow" ? 150 : 100;
    if (data->state->reverse_creation_order) {
      delay_ms = data->name == "slow" ? 100 : 200;
    }
    workers_.emplace_back([this, data, delay_ms]() {
      ten_sleep_ms(delay_ms);
      int rc = ten_runloop_post_task_tail(
          loop_,
          [](void *, void *arg) {
            std::unique_ptr<completion> data(static_cast<completion *>(arg));
            auto *extension =
                data->name == "failed"
                    ? nullptr
                    : new startup_extension(data->name.c_str(), data->state);
            data->ten_env->on_create_instance_done(extension, data->context);
          },
          nullptr, data);
      TEN_ASSERT(rc == 0, "Failed to complete graph startup test creation.");
    });
  }

  void on_destroy_instance(ten::ten_env_t &ten_env, void *instance,
                           void *context) override {
    delete static_cast<startup_extension *>(instance);
    ten_env.on_destroy_instance_done(context);
  }

 private:
  ten_runloop_t *loop_;
  std::vector<std::thread> workers_;
};

void register_startup_addon(ten_addon_registration_t *,
                            ten_addon_registration_done_func_t done,
                            ten_addon_register_ctx_t *register_ctx,
                            void *user_data) {
  if (!ten_addon_is_registered(register_ctx, TEN_ADDON_TYPE_EXTENSION,
                               startup_addon_name)) {
    auto *addon =
        new startup_addon(ten_app_get_attached_runloop(register_ctx->app));
    ten_addon_register_extension(
        startup_addon_name, nullptr,
        static_cast<ten_addon_t *>(addon->get_c_instance()), register_ctx);
  }
  done(register_ctx, user_data);
}

TEN_CONSTRUCTOR(register_graph_startup_duration_test) {
  bool rc = ten_addon_manager_add_addon(
      ten_addon_manager_get_instance(), "extension", startup_addon_name,
      register_startup_addon, nullptr, nullptr);
  TEN_ASSERT(rc, "Failed to register graph startup test addon.");
}

void configure_startup_app(ten_app_t *app, ten_env_t *ten_env) {
  auto *state = static_cast<startup_state *>(app->user_data);
  nlohmann::json config = {
      {"ten",
       {{"uri", "msgpack://127.0.0.1:8001/"},
        {"one_event_loop_per_engine", state->own_engine_loop},
        {"services",
         {{"telemetry",
           {{"enabled", true}, {"metrics", {{"enabled", true}}}}}}}}}};
  if (state->auto_start) {
    config["ten"]["predefined_graphs"] = {{{"name", "startup_graph"},
                                           {"auto_start", true},
                                           {"singleton", true},
                                           {"graph", startup_graph()}}};
  }
  EXPECT_TRUE(
      ten_env_init_property_from_json(ten_env, config.dump().c_str(), nullptr));
  ten_env_on_configure_done(ten_env, nullptr);
}

void deinit_startup_app(ten_app_t *app, ten_env_t *ten_env) {
  auto *value = ten_env_peek_property(ten_env, "ten:runtime_stats", nullptr);
  EXPECT_NE(value, nullptr);
  if (value) {
    ten_json_t json = TEN_JSON_INIT_VAL(ten_json_create_new_ctx(), true);
    EXPECT_TRUE(ten_value_to_json(value, &json));
    bool must_free = false;
    const char *text = ten_json_to_string(&json, nullptr, &must_free);
    static_cast<startup_state *>(app->user_data)->final_stats =
        nlohmann::json::parse(text);
    if (must_free) {
      TEN_FREE(text);
    }
    ten_json_deinit(&json);
  }
  ten_env_on_deinit_done(ten_env, nullptr);
}

void *startup_app_main(void *arg) {
  auto *app = ten_app_create(configure_startup_app, nullptr, deinit_startup_app,
                             nullptr);
  app->user_data = arg;
  ten_app_run(app, false, nullptr);
  ten_app_destroy(app);
  return nullptr;
}

nlohmann::json startup_points(const nlohmann::json &stats) {
  auto points = nlohmann::json::array();
  for (const auto &resource : stats["metrics"]["resourceMetrics"]) {
    for (const auto &scope : resource["scopeMetrics"]) {
      for (const auto &metric : scope["metrics"]) {
        if (metric["name"] == "graph_startup_duration") {
          EXPECT_TRUE(metric["gauge"].is_object());
          for (const auto &point : metric["gauge"]["dataPoints"]) {
            points.push_back(point);
          }
        }
      }
    }
  }
  return points;
}

nlohmann::json read_startup_stats(ten::msgpack_tcp_client_t &client,
                                  const std::string &graph_id) {
  auto cmd = ten::cmd_t::create("stats");
  cmd->set_dests({{"msgpack://127.0.0.1:8001/", graph_id.c_str(), "observer"}});
  auto result = client.send_cmd_and_recv_result(std::move(cmd));
  ten_test::check_status_code(result, TEN_STATUS_CODE_OK);
  return nlohmann::json::parse(result->get_property_string("detail"));
}

void check_graph_startup(bool own_engine_loop, bool auto_start,
                         bool reverse_creation_order = false) {
  startup_state state;
  state.own_engine_loop = own_engine_loop;
  state.auto_start = auto_start;
  state.reverse_creation_order = reverse_creation_order;
  active_startup_state = &state;
  auto *thread =
      ten_thread_create("graph startup app", startup_app_main, &state);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    std::string graph_id = "startup_graph";
    int64_t sent_us = 0;
    if (!auto_start) {
      auto cmd = ten::start_graph_cmd_t::create();
      cmd->set_graph_from_json(startup_graph().dump().c_str());
      // Command construction and sender-side waiting are outside the metric.
      ten_sleep_ms(200);
      sent_us = ten_current_time_us();
      auto result = client.send_cmd_and_recv_result(std::move(cmd));
      ten_test::check_status_code(result, TEN_STATUS_CODE_OK);
      graph_id = result->get_property_string("graph_id");
    }

    EXPECT_EQ(ten_event_wait(state.configure_entered, 5000), 0);
    EXPECT_FALSE(state.configure_done.load());
    auto first = startup_points(read_startup_stats(client, graph_id));
    EXPECT_EQ(first.size(), 1);
    if (first.size() == 1) {
      EXPECT_GE(first[0]["asDouble"].get<double>(), 150000);
      if (!auto_start) {
        EXPECT_LE(first[0]["asDouble"].get<double>(),
                  state.latest_configure_started_us.load() - sent_us);
      }
      std::map<std::string, std::string> attributes;
      for (const auto &attribute : first[0]["attributes"]) {
        attributes[attribute["key"].get<std::string>()] =
            attribute["value"]["stringValue"];
      }
      EXPECT_EQ(attributes["app_uri"], "msgpack://127.0.0.1:8001/");
      EXPECT_FALSE(attributes["graph_id"].empty());
      if (!auto_start) {
        EXPECT_EQ(attributes["graph_id"], graph_id);
      }
    }

    if (!auto_start) {
      auto duplicate = ten::start_graph_cmd_t::create();
      duplicate->set_graph_from_json(startup_graph().dump().c_str());
      duplicate->set_dests({{"msgpack://127.0.0.1:8001/", graph_id.c_str()}});
      auto result = client.send_cmd_and_recv_result(std::move(duplicate));
      // A duplicate may be rejected; either response must leave the original
      // graph's startup sample unchanged.
      EXPECT_NE(result, nullptr);
    }
    ten_sleep_ms(100);
    auto second = startup_points(read_startup_stats(client, graph_id));
    EXPECT_EQ(second.size(), first.size());
    if (first.size() == 1 && second.size() == 1) {
      EXPECT_EQ(second[0]["asDouble"], first[0]["asDouble"]);
    }
    EXPECT_FALSE(state.configure_done.load());
    ten_event_set(state.release_configure);
  }
  ten_thread_join(thread, -1);
  EXPECT_TRUE(state.configure_done.load());
  active_startup_state = nullptr;
}

}  // namespace

TEST(RuntimeStatsTest, GraphStartupEndsBeforeLifecycleCallbacks) {
  check_graph_startup(false, false);
}

TEST(RuntimeStatsTest, GraphStartupWithOwnEngineLoop) {
  check_graph_startup(true, false);
}

TEST(RuntimeStatsTest, GraphStartupForAutoStartGraph) {
  check_graph_startup(false, true);
}

TEST(RuntimeStatsTest, GraphStartupHandlesEitherGroupFinishingLast) {
  check_graph_startup(false, false, true);
}

TEST(RuntimeStatsTest, GraphStartupSkipsFailedCreation) {
  startup_state state;
  active_startup_state = &state;
  auto *thread =
      ten_thread_create("failed graph startup app", startup_app_main, &state);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    auto cmd = ten::start_graph_cmd_t::create();
    cmd->set_graph_from_json(R"({"nodes":[{
      "type":"extension","name":"failed","addon":"graph_startup_duration_test","extension_group":"worker"
    }]})");
    auto result = client.send_cmd_and_recv_result(std::move(cmd));
    ten_test::check_status_code(result, TEN_STATUS_CODE_ERROR);
  }
  ten_thread_join(thread, -1);
  EXPECT_TRUE(startup_points(state.final_stats).empty());
  active_startup_state = nullptr;
}

#endif
