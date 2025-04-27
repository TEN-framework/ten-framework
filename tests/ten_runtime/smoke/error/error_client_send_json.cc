//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <string>

#include "gtest/gtest.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/start_graph.h"
#include "ten_runtime/msg/cmd/start_graph/cmd.h"
#include "ten_runtime/ten_env/internal/metadata.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/lib/thread.h"
#include "tests/common/client/msgpack_tcp.h"

namespace {

class test_extension_1 : public ten::extension_t {
 public:
  explicit test_extension_1(const char *name) : extension_t(name) {}
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(error_client_send_json__extension_1,
                                    test_extension_1);

class test_extension_2 : public ten::extension_t {
 public:
  explicit test_extension_2(const char *name) : ten::extension_t(name) {}
};

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(error_client_send_json__extension_2,
                                    test_extension_2);

void test_app_on_configure(TEN_UNUSED ten_app_t *self, ten_env_t *ten_env) {
  bool result = ten_env_init_property_from_json(ten_env,
                                                "{\
                          \"ten\": {\
                          \"uri\": \"msgpack://127.0.0.1:8001/\",\
                          \"log\": {\
                            \"level\": 2\
                          }\
                          }\
                         }",
                                                nullptr);
  ASSERT_EQ(result, true);

  ten_env_on_configure_done(ten_env, nullptr);
}

void *test_app_thread_main(TEN_UNUSED void *args) {
  ten_app_t *app =
      ten_app_create(test_app_on_configure, nullptr, nullptr, nullptr);
  ten_app_run(app, false, nullptr);
  ten_app_wait(app, nullptr);
  ten_app_destroy(app);

  return nullptr;
}

}  // namespace

TEST(ExtensionTest, ErrorClientSendJson) {  // NOLINT
  auto *app_thread =
      ten_thread_create("test_app_thread_main", test_app_thread_main, nullptr);

  // Create a client and connect to the app.
  ten_test_msgpack_tcp_client_t *client =
      ten_test_msgpack_tcp_client_create("msgpack://127.0.0.1:8001/");

  ten_error_t *err = ten_error_create();
  auto *invalid_graph_cmd = ten_cmd_start_graph_create();
  bool success =
      ten_cmd_start_graph_set_graph_from_json_str(invalid_graph_cmd, R"({
        "nodes":[
          {
            "type": "extension",
            "name": "extension_1",
            "addon": "error_client_send_json__extension_1",
            "app": "msgpack://127.0.0.1:8001/",
            "extension_group": "extension_group"
          },
          {
            "type": "extension",
            "name": "extension_1",
            "addon": "error_client_send_json__extension_2",
            "app": "msgpack://127.0.0.1:8001/",
            "extension_group": "extension_group"
          }
        ]
      })",
                                                  err);
  EXPECT_EQ(success, false);
  EXPECT_STREQ(ten_error_message(err),
               "extension 'extension_1' is associated with different addon "
               "'error_client_send_json__extension_2', "
               "'error_client_send_json__extension_1'");

  ten_error_destroy(err);
  ten_shared_ptr_destroy(invalid_graph_cmd);

  // Strange connection would _not_ cause the TEN app to be closed, so we have
  // to close the TEN app explicitly.
  ten_test_msgpack_tcp_client_close_app(client);

  ten_test_msgpack_tcp_client_destroy(client);

  ten_thread_join(app_thread, -1);
}
