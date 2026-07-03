//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

namespace {

class test_normal_extension_1 : public ten::extension_t {
 public:
  explicit test_normal_extension_1(const char *name) : ten::extension_t(name) {}
};

class test_normal_extension_2 : public ten::extension_t {
 public:
  explicit test_normal_extension_2(const char *name) : ten::extension_t(name) {}
};

class test_normal_extension_3 : public ten::extension_t {
 public:
  explicit test_normal_extension_3(const char *name) : ten::extension_t(name) {}
};

class test_predefined_graph : public ten::extension_t {
 public:
  explicit test_predefined_graph(const char *name) : ten::extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    auto start_graph_cmd = ten::start_graph_cmd_t::create();
    start_graph_cmd->set_dests({{""}});
    start_graph_cmd->set_graph_from_json(
        R"({
  "nodes": [
    {
      "type": "extension",
      "name": "main",
      "addon": "start_graph_with_complex_connection__normal_extension_1",
      "extension_group": "main"
    },
    {
      "type": "extension",
      "name": "rtc",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "rtc"
    },
    {
      "type": "extension",
      "name": "agora_audio3a_downstream",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "agora_audio3a_downstream"
    },
    {
      "type": "extension",
      "name": "agora_audio3a_upstream",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "agora_audio3a_upstream"
    },
    {
      "type": "extension",
      "name": "agora_sess_ctrl",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "agora_sess_ctrl"
    },
    {
      "type": "extension",
      "name": "tts",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "tts"
    },

    {
      "type": "extension",
      "name": "llm",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "llm"
    },
    {
      "type": "extension",
      "name": "turn_detector",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "turn_detector"
    },
    {
      "type": "extension",
      "name": "asr",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "asr"
    },
    {
      "type": "extension",
      "name": "rtm",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "rtm"
    },
    {
      "type": "extension",
      "name": "context",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "context"
    },
    {
      "type": "extension",
      "name": "state_python",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "state_python"
    },
    {
      "type": "extension",
      "name": "tts_input_transfer_extension",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "tts_input_transfer_extension"
    },
    {
      "type": "extension",
      "name": "tts_output_transfer_extension",
      "addon": "start_graph_with_complex_connection__normal_extension_2",
      "extension_group": "tts_output_transfer_extension"
    }
  ],
  "connections": [
    {
      "audio_frame": [
        {
          "name": "pcm_frame",
          "source": [
            { "extension": "rtc" },
            { "extension": "agora_audio3a_downstream" },
            { "extension": "agora_sess_ctrl" },
            { "extension": "tts" },
            { "extension": "llm" },
            { "extension": "agora_audio3a_upstream" }
          ]
        }
      ],
      "cmd": [
        {
          "names": [
            "on_connected",
            "on_user_joined",
            "on_connection_error",
            "on_connection_failure",
            "on_connection_lost",
            "on_user_left",
            "on_subscribed_remote_users_changed",
            "on_user_track_state_unsubscribed"
          ],
          "source": [
            { "extension": "rtc" }
          ]
        }
      ],
      "data": [
        {
          "names": [
            "asr_result",
            "asr_finalize_end"
          ],
          "source": [
            { "extension": "asr" }
          ]
        },
        {
          "names": [
            "sos",
            "eos"
          ],
          "source": [
            { "extension": "agora_sess_ctrl" }
          ]
        },
        {
          "names": [
            "tts_text_result",
            "tts_audio_start",
            "tts_audio_end",
            "tts_flush_end"
          ],
          "source": [
            { "extension": "tts" }
          ]
        },
        {
          "names": [
            "rtm_message_event",
            "rtm_presence_event"
          ],
          "source": [
            { "extension": "rtm" }
          ]
        },
        {
          "name": "text_data",
          "source": [
            { "extension": "context" },
            { "extension": "llm" },
            { "extension": "turn_detector" },
            { "extension": "tts_input_transfer_extension" },
            { "extension": "tts_output_transfer_extension" }
          ]
        },
        {
          "names": [
            "on_listen_end",
            "on_think_start",
            "on_tts_start",
            "on_tts_end",
            "on_interrupt",
            "on_think_end"
          ],
          "source": [
            { "extension": "llm" }
          ]
        },
        {
          "name": "state_change",
          "source": [
            { "extension": "state_python" }
          ]
        },
        {
          "name": "start_of_turn",
          "source": [
            { "extension": "turn_detector" }
          ]
        },
        {
          "names": [
            "tts_flush",
            "tts_text_input"
          ],
          "source": [
            { "extension": "tts_input_transfer_extension" }
          ]
        },
        {
          "names": [
            "on_tts_start",
            "on_tts_end"
          ],
          "source": [
            { "extension": "tts_output_transfer_extension" }
          ]
        },
        {
          "names": [
            "chat_completion",
            "set_metadata",
            "on_interrupt",
            "start_of_turn"
          ],
          "source": [
            { "extension": "context" }
          ]
        }
      ],
      "extension": "main"
    }
  ]
})"_json.dump()
            .c_str());

    ten_env.send_cmd(
        std::move(start_graph_cmd),
        [this](ten::ten_env_t &ten_env,
               std::unique_ptr<ten::cmd_result_t> cmd_result,
               ten::error_t * /* err */) {
          // result for the 'start_graph' command
          auto graph_id = cmd_result->get_property_string("graph_id");

          // Shut down the graph; otherwise, the app won't be able to close
          // because there is still a running engine/graph.
          auto stop_graph_cmd = ten::stop_graph_cmd_t::create();
          stop_graph_cmd->set_dests({{""}});
          stop_graph_cmd->set_graph_id(graph_id.c_str());

          ten_env.send_cmd(
              std::move(stop_graph_cmd),
              [this](ten::ten_env_t &ten_env,
                     std::unique_ptr<ten::cmd_result_t> /* cmd_result */,
                     ten::error_t * /* err */) {
                start_graph_cmd_is_done = true;

                if (test_cmd != nullptr) {
                  nlohmann::json detail = {{"id", 1}, {"name", "a"}};

                  auto cmd_result_for_test =
                      ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *test_cmd);
                  cmd_result_for_test->set_property_from_json(
                      "detail", detail.dump().c_str());
                  ten_env.return_result(std::move(cmd_result_for_test));
                }
              });
        });

    ten_env.on_start_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "test") {
      if (start_graph_cmd_is_done) {
        nlohmann::json detail = {{"id", 1}, {"name", "a"}};

        auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
        cmd_result->set_property_from_json("detail", detail.dump().c_str());
        ten_env.return_result(std::move(cmd_result));
      } else {
        test_cmd = std::move(cmd);
        return;
      }
    } else {
      TEN_ASSERT(0, "Should not happen.");
    }
  }

 private:
  bool start_graph_cmd_is_done{};
  std::unique_ptr<ten::cmd_t> test_cmd;
};

class test_app : public ten::app_t {
 public:
  void on_configure(ten::ten_env_t &ten_env) override {
    bool rc = ten::ten_env_internal_accessor_t::init_manifest_from_json(
        ten_env,
        // clang-format off
                 R"({
                      "type": "app",
                      "name": "test_app",
                      "version": "0.1.0"
                    })"
        // clang-format on
    );
    ASSERT_EQ(rc, true);

    rc = ten_env.init_property_from_json(
        // clang-format off
                 R"({
                      "ten": {
                        "uri": "msgpack://127.0.0.1:8001/",
                        "log": {
                          "handlers": [
                            {
                              "matchers": [
                                {
                                  "level": "debug"
                                }
                              ],
                              "formatter": {
                                "type": "plain",
                                "colored": true
                              },
                              "emitter": {
                                "type": "console",
                                "config": {
                                  "stream": "stdout"
                                }
                              }
                            }
                          ]
                        },
                        "predefined_graphs": [{
                          "name": "default",
                          "auto_start": false,
                          "singleton": true,
                          "graph": {
                            "nodes": [{
                              "type": "extension",
                              "name": "predefined_graph",
                              "addon": "start_graph_with_complex_connection__predefined_graph_extension",
                              "extension_group": "start_graph_with_complex_connection__predefined_graph_group"
                            }]
                          }
                        }]
                      }
                    })"
        // clang-format on
    );
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

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    start_graph_with_complex_connection__predefined_graph_extension,
    test_predefined_graph);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    start_graph_with_complex_connection__normal_extension_1,
    test_normal_extension_1);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    start_graph_with_complex_connection__normal_extension_2,
    test_normal_extension_2);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(
    start_graph_with_complex_connection__normal_extension_3,
    test_normal_extension_3);

}  // namespace

TEST(GraphSyntaxSugarTest, StartGraphWithComplexConnection) {  // NOLINT
  auto *app_thread = ten_thread_create("app thread", app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Do not need to send 'start_graph' command first.
  // The 'graph_id' MUST be "default" (a special string) if we want to send the
  // request to predefined graph.
  auto test_cmd = ten::cmd_t::create("test");
  test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "predefined_graph"}});
  auto cmd_result = client->send_cmd_and_recv_result(std::move(test_cmd));
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_json(cmd_result, R"({"id": 1, "name": "a"})");

  delete client;

  ten_thread_join(app_thread, -1);
}
