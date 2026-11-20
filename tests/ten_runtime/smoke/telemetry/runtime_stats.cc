//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cstdio>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/value/value_get.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

struct runtime_stats_app_config {
  bool telemetry;
  nlohmann::json exporter;
};

class runtime_stats_extension : public ten::extension_t {
 public:
  explicit runtime_stats_extension(const char *name) : ten::extension_t(name) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    EXPECT_TRUE(ten_env.init_property_from_json(R"({"ordinary":7})"));
    ten_env.on_configure_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    auto result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    if (cmd->get_name() == "stats") {
      result->set_property("detail",
                           ten_env.get_property_to_json("ten:runtime_stats"));
    } else if (cmd->get_name() == "property_checks") {
      EXPECT_EQ(ten_env.get_property_int32("ordinary"), 7);
      auto configured = nlohmann::json::parse(ten_env.get_property_to_json(""));
      EXPECT_EQ(configured.size(), 1);
      EXPECT_EQ(configured["ordinary"], 7);

      ten::error_t err;
      EXPECT_FALSE(
          ten_env.set_property_from_json("ten:runtime_stats", "{}", &err));
      EXPECT_EQ(err.error_code(), TEN_ERROR_CODE_INVALID_ARGUMENT);
      EXPECT_FALSE(ten_env.set_property_from_json("ten:runtime_stats.metrics",
                                                  "{}", &err));

      EXPECT_EQ(ten_env.get_property_to_json(
                    "ten:runtime_stats.metrics.missing", &err),
                "");
      EXPECT_FALSE(err.is_success());

      auto metrics_json =
          ten_env.get_property_to_json("ten:runtime_stats.metrics");
      auto metrics = nlohmann::json::parse(metrics_json);
      if (!metrics["resourceMetrics"].empty()) {
        EXPECT_EQ(ten_env.get_property_string(
                      "ten:runtime_stats.metrics.resourceMetrics[0]."
                      "scopeMetrics[0].scope.name"),
                  "ten-framework");
      }
      result->set_property("detail", metrics_json);
    }
    ten_env.return_result(std::move(result));
  }
};

class runtime_stats_app : public ten::app_t {
 public:
  explicit runtime_stats_app(const runtime_stats_app_config &config)
      : config_(config) {}

  void on_configure(ten::ten_env_t &ten_env) override {
    nlohmann::json config = {{"ten", {{"uri", "msgpack://127.0.0.1:8001/"}}}};
    if (config_.telemetry) {
      config["ten"]["services"]["telemetry"] = {
          {"enabled", true}, {"metrics", {{"enabled", true}}}};
      if (!config_.exporter.is_null()) {
        config["ten"]["services"]["telemetry"]["metrics"]["exporter"] =
            config_.exporter;
      }
    }
    EXPECT_TRUE(ten_env.init_property_from_json(config.dump().c_str()));
    auto stats = ten_env.get_property_to_json("ten:runtime_stats");
    EXPECT_FALSE(stats.empty());
    ten_env.on_configure_done();
  }

 private:
  runtime_stats_app_config config_;
};

void *app_thread_main(void *arg) {
  runtime_stats_app app(*static_cast<runtime_stats_app_config *>(arg));
  app.run();
  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(runtime_stats_test_extension,
                                    runtime_stats_extension);

void check_runtime_stats(bool telemetry,
                         const nlohmann::json &exporter = nullptr) {
  runtime_stats_app_config config{telemetry, exporter};
  std::string response_json;
  auto *thread =
      ten_thread_create("runtime stats app", app_thread_main, &config);
  {
    ten::msgpack_tcp_client_t client("msgpack://127.0.0.1:8001/");
    auto graph = ten::start_graph_cmd_t::create();
    graph->set_graph_from_json(R"({"nodes":[{
      "type":"extension", "name":"worker",
      "addon":"runtime_stats_test_extension", "extension_group":"worker"
    }]})");
    auto result = client.send_cmd_and_recv_result(std::move(graph));
    ten_test::check_status_code(result, TEN_STATUS_CODE_OK);

    auto send = [&client](const char *name) {
      auto cmd = ten::cmd_t::create(name);
      cmd->set_dests({{"msgpack://127.0.0.1:8001/", "", "worker"}});
      return client.send_cmd_and_recv_result(std::move(cmd));
    };

    send("hello_world");
    send("hello_world");
    result = send("stats");
    ten_test::check_status_code(result, TEN_STATUS_CODE_OK);
    auto detail = result->get_property_string("detail");
    EXPECT_FALSE(detail.empty());
    if (!detail.empty()) {
      auto stats = nlohmann::json::parse(detail);
      response_json = stats.dump(2);
      EXPECT_TRUE(stats["metrics"]["resourceMetrics"].is_array());
#if defined(TEN_ENABLE_TEN_RUST_APIS)
      if (telemetry) {
        bool found = false;
        for (const auto &resource : stats["metrics"]["resourceMetrics"]) {
          for (const auto &scope : resource["scopeMetrics"]) {
            for (const auto &metric : scope["metrics"]) {
              if (metric["name"] != "extension_cmd_processing_duration") {
                continue;
              }
              for (const auto &point : metric["histogram"]["dataPoints"]) {
                for (const auto &attribute : point["attributes"]) {
                  if (attribute["key"] == "msg_name" &&
                      attribute["value"]["stringValue"] == "hello_world") {
                    EXPECT_EQ(point["count"], 2);
                    EXPECT_TRUE(point["sum"].is_number());
                    EXPECT_EQ(point["bucketCounts"].size(),
                              point["explicitBounds"].size() + 1);
                    found = true;
                  }
                }
              }
            }
          }
        }
        EXPECT_TRUE(found);
      } else
#endif
      {
        EXPECT_TRUE(stats["metrics"]["resourceMetrics"].empty());
      }

      result = send("property_checks");
      auto metrics =
          nlohmann::json::parse(result->get_property_string("detail"));
      EXPECT_TRUE(metrics["resourceMetrics"].is_array());
    }
  }
  ten_thread_join(thread, -1);
  if (!response_json.empty()) {
    std::printf(
        "\nget_property_to_json(\"ten:runtime_stats\") returned:\n%s\n\n",
        response_json.c_str());
    std::fflush(stdout);
  }
}

}  // namespace

TEST(RuntimeStatsTest, ReadsSdkMetricsThroughProperties) {
  check_runtime_stats(true);
}

TEST(RuntimeStatsTest, ReturnsEmptyMetricsWithoutTelemetry) {
  check_runtime_stats(false);
}

TEST(RuntimeStatsTest, ReadsSdkMetricsWithOtlpWithoutEndpoint) {
  check_runtime_stats(true, {{"type", "otlp"}});
}

TEST(RuntimeStatsTest, ReadsSdkMetricsWithOtlpEmptyEndpoint) {
  check_runtime_stats(true, {{"type", "otlp"}, {"config", {{"endpoint", ""}}}});
}

TEST(RuntimeStatsTest, ReadsSdkMetricsWithOtlpInvalidEndpoint) {
  check_runtime_stats(
      true, {{"type", "otlp"}, {"config", {{"endpoint", "not-a-url"}}}});
}
