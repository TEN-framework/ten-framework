//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <nlohmann/json.hpp>
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/common/status_code.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/cpp/msgpack_tcp.h"
#include "tests/ten_runtime/smoke/util/binding/cpp/check.h"

// This test demonstrates the usage of get_source() and set_dests() methods
// for data_t messages. The flow is:
//
// client → extension_1 → extension_2 → extension_1 → client
//
// 1. Extension_1 sends data_1 to extension_2
// 2. Extension_2 receives data_1, gets its source using get_source()
// 3. Extension_2 creates data_2 and uses the source of data_1 as the
//    destination for data_2 using set_dests()
// 4. Extension_1 receives data_2 and confirms the test is successful

namespace {

class source_extension : public ten::extension_t {
 public:
  explicit source_extension(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    if (cmd->get_name() == "start_test") {
      start_test_cmd = std::move(cmd);

      // Create and send data_1 to destination extension
      auto data_1 = ten::data_t::create("data_1");
      data_1->set_property("step", 1);
      data_1->set_property("message", "first data from source");

      bool set_dest_success =
          data_1->set_dests({{nullptr, "", "destination_extension"}});
      TEN_ASSERT(set_dest_success == false, "app_uri is empty is an error");

      // Explicitly specify to send to destination extension
      set_dest_success = data_1->set_dests({{"", "", "destination_extension"}});
      TEN_ASSERT(set_dest_success == true, "should success");

      ten_env.send_data(std::move(data_1));
    }
  }

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    if (data->get_name() == "data_2") {
      // Received data_2 back from destination extension
      // This means the test is successful
      if (data->get_property_int64("step") == 2 &&
          data->get_property_string("message") ==
              "second data returned to source") {
        // Return success result to client
        auto cmd_result =
            ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *start_test_cmd);
        cmd_result->set_property("detail", "test_success");
        ten_env.return_result(std::move(cmd_result));
      } else {
        // Return failure result
        auto cmd_result =
            ten::cmd_result_t::create(TEN_STATUS_CODE_ERROR, *start_test_cmd);
        cmd_result->set_property("detail", "test_failed");
        ten_env.return_result(std::move(cmd_result));
      }
    }
  }

 private:
  std::unique_ptr<ten::cmd_t> start_test_cmd;
};

class destination_extension : public ten::extension_t {
 public:
  explicit destination_extension(const char *name) : ten::extension_t(name) {}

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    if (data->get_name() == "data_1") {
      // Received data_1 from source extension
      // Get the source location of data_1
      ten::loc_t source_loc = data->get_source();

      // Create data_2 and set its destination to the source of data_1
      auto data_2 = ten::data_t::create("data_2");
      data_2->set_property("step", 2);
      data_2->set_property("message", "second data returned to source");

      // Use the source of data_1 as the destination for data_2
      data_2->set_dests({source_loc});

      ten_env.send_data(std::move(data_2));
    }
  }
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
                 "auto_start": true,
                 "singleton": true,
                 "graph": {
                   "nodes": [{
                     "type": "extension",
                     "name": "source_extension",
                     "addon": "source_to_dest_data__source_extension",
                     "extension_group": "source_to_dest_data_group"
                   },{
                     "type": "extension",
                     "name": "destination_extension",
                     "addon": "source_to_dest_data__destination_extension",
                     "extension_group": "source_to_dest_data_group"
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

void *test_app_thread_main(TEN_UNUSED void *args) {
  auto *app = new test_app();
  app->run();
  delete app;

  return nullptr;
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(source_to_dest_data__source_extension,
                                    source_extension);
TEN_CPP_REGISTER_ADDON_AS_EXTENSION(source_to_dest_data__destination_extension,
                                    destination_extension);

}  // namespace

TEST(ExtensionTest, SourceToDestData2) {  // NOLINT
  // Start the app.
  auto *app_thread =
      ten_thread_create("test app thread", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  auto *client = new ten::msgpack_tcp_client_t("msgpack://127.0.0.1:8001/");

  // Send the "start_test" command to the source extension.
  auto start_test_cmd = ten::cmd_t::create("start_test");
  start_test_cmd->set_dests(
      {{"msgpack://127.0.0.1:8001/", "default", "source_extension"}});
  auto cmd_result = client->send_cmd_and_recv_result(std::move(start_test_cmd));

  // Check whether the correct result has been received.
  ten_test::check_status_code(cmd_result, TEN_STATUS_CODE_OK);
  ten_test::check_detail_with_string(cmd_result, "test_success");

  delete client;

  ten_thread_join(app_thread, -1);
}
