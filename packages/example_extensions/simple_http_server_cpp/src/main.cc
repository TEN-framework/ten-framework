//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include <libwebsockets.h>

#include <atomic>
#include <cstddef>
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "include_internal/ten_utils/lib/buf.h"
#include "ten_utils/lib/buf.h"

#define DEFAULT_BUF_CAPACITY 512
#define DEFAULT_SERVER_URL "127.0.0.1"
#define DEFAULT_SERVER_PORT 8001

namespace {

struct http_server_t;
class http_server_extension_t;

enum HTTP_METHOD {
  HTTP_METHOD_INVALID,

  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
  HTTP_METHOD_PUT,
  HTTP_METHOD_PATCH,
  HTTP_METHOD_DELETE,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_OPTIONS,
};

// Represents the data of one http transaction (the duration between the
// protocol binding and unbinding).
struct http_transaction_data_t {
  http_transaction_data_t() = default;
  ~http_transaction_data_t() {
    if (req_buf != nullptr) {
      ten_buf_destroy(req_buf);
      req_buf = nullptr;
    }

    if (resp_buf != nullptr) {
      ten_buf_destroy(resp_buf);
      resp_buf = nullptr;
    }

    wsi = nullptr;
    lws_context = nullptr;
    http_server = nullptr;
  }

  // @{
  http_transaction_data_t(const http_transaction_data_t &other) = delete;
  http_transaction_data_t &operator=(const http_transaction_data_t &cmd) =
      delete;
  http_transaction_data_t(http_transaction_data_t &&other) = delete;
  http_transaction_data_t &operator=(http_transaction_data_t &&cmd) = delete;
  // @}

  ten_buf_t *req_buf{nullptr};   // HTTP request of the session.
  ten_buf_t *resp_buf{nullptr};  // HTTP response of the above request.
  struct lws *wsi{nullptr};      // wsi of the session.
  struct lws_context *lws_context{nullptr};
  http_server_t *http_server{nullptr};
  HTTP_METHOD method{HTTP_METHOD_INVALID};
  std::string url;
};

enum react_ten_stopping_state_t {
  REACT_TEN_STOPPING_NOT_START,
  REACT_TEN_STOPPING,
  REACT_TEN_STOPPING_COMPLETED,
};

// Represents a http server.
struct http_server_t {
  struct lws_context *lws_context{nullptr};
  ten::ten_env_proxy_t *ten_env_proxy{nullptr};

  std::atomic<react_ten_stopping_state_t> react_ten_stopping_state{
      REACT_TEN_STOPPING_NOT_START};

  std::list<std::shared_ptr<http_transaction_data_t>> all_http_session_data;

  std::thread http_server_thread;
};

// Write the response header to the client.
void return_response_header(
    const std::shared_ptr<http_transaction_data_t> &http_session_data,
    struct lws *wsi) {
  assert(wsi && http_session_data && "Invalid argument.");

  ten_buf_t header;
  ten_buf_init_with_owned_data(&header, DEFAULT_BUF_CAPACITY);

  unsigned char *start = static_cast<unsigned char *>(header.data) + LWS_PRE;
  unsigned char *p = start;
  unsigned char *end =
      static_cast<unsigned char *>(header.data) + header.size - 1;

  int rc = lws_add_http_common_headers(
      wsi, HTTP_STATUS_OK, "application/json",
      ten_buf_get_content_size(http_session_data->resp_buf), &p, end);
  assert(rc == 0 && "Failed to return a http response header.");

  rc = lws_finalize_http_header(wsi, &p, end);
  assert(rc == 0 && "Failed to return a http response header.");

  rc = lws_write(wsi, start, p - start, LWS_WRITE_HTTP_HEADERS);
  if (rc < p - start) {
    TEN_ASSERT(0, "Failed to return a http response header: (%zu)%d", p - start,
               rc);
  }

  ten_buf_deinit(&header);
}

// Write the response body to the client.
void return_response_body(
    const std::shared_ptr<http_transaction_data_t> &http_session_data,
    struct lws *wsi) {
  assert(wsi && http_session_data && "Invalid argument.");

  // Allocate a buffer to hold the response body plus LWS_PRE header space.
  ten_buf_t body;
  ten_buf_init_with_owned_data(
      &body, ten_buf_get_content_size(http_session_data->resp_buf) + LWS_PRE);

  // Copy the response body to the buffer.
  unsigned char *start = static_cast<unsigned char *>(body.data) + LWS_PRE;
  memcpy(start, http_session_data->resp_buf->data,
         ten_buf_get_content_size(http_session_data->resp_buf));

  // Write out the buffer.
  int rc = lws_write(wsi, start,
                     ten_buf_get_content_size(http_session_data->resp_buf),
                     LWS_WRITE_HTTP_FINAL);
  if (rc <
      static_cast<int>(ten_buf_get_content_size(http_session_data->resp_buf))) {
    TEN_ASSERT(0, "Failed to return a http response body: (%zu)%d",
               ten_buf_get_content_size(http_session_data->resp_buf), rc);
  }

  ten_buf_deinit(&body);
}

// Write the response (header & body) to the client.
void return_response(
    const std::shared_ptr<http_transaction_data_t> &http_session_data,
    struct lws *wsi) {
  assert(wsi && http_session_data && "Invalid argument.");

  return_response_header(http_session_data, wsi);
  return_response_body(http_session_data, wsi);
}

void prepare_response_data(
    const std::shared_ptr<http_transaction_data_t> &http_session_data,
    const std::string &resp) {
  assert(http_session_data && "Invalid argument.");
  assert(http_session_data->resp_buf && "Should not happen.");

  // Put the response data into the http_session_data. The access times for
  // resp_buf are staggered, therefore there is no need to protect resp_buf.
  ten_buf_reset(http_session_data->resp_buf);
  ten_buf_push(http_session_data->resp_buf, (uint8_t *)(resp.c_str()),
               resp.length());
}

void prepare_response_data_from_ten_world(
    const std::shared_ptr<http_transaction_data_t> &http_session_data,
    const std::string &resp) {
  assert(http_session_data && "Invalid argument.");
  assert(http_session_data->resp_buf && "Should not happen.");

  prepare_response_data(http_session_data, resp);

  // Notify the lws world that resp_buf has data to be processed.
  lws_cancel_service(http_session_data->lws_context);
}

bool trigger_lws_write_out_timing(http_server_t *http_server) {
  assert(http_server && "Invalid argument.");

  bool has_pending_resp = !http_server->all_http_session_data.empty();
  for (auto &&http_session_data : http_server->all_http_session_data) {
    if (ten_buf_get_content_size(http_session_data->resp_buf) == 0 &&
        http_server->react_ten_stopping_state == REACT_TEN_STOPPING) {
      // TEN is stopping, for all HTTP requests that have not yet received an
      // official TEN response, uniformly respond with a default close message.
      prepare_response_data(http_session_data, "TEN is closed.");
    }

    if (ten_buf_get_content_size(http_session_data->resp_buf) > 0) {
      // Trigger the lws world to write out the response data.
      lws_callback_on_writable(http_session_data->wsi);
    }
  }

  return has_pending_resp;
}

// This function would be executed in the TEN world.
void proceed_to_stop_http_extension(ten::ten_env_t &ten_env,
                                    http_server_t *http_server) {
  assert(http_server && "Invalid argument.");

  // Wait libws thread to stop completely and reclaim it.
  if (http_server->http_server_thread.joinable()) {
    http_server->http_server_thread.join();
  }

  // Reclaim ten proxy so that the TEN world can continue to stop.
  delete http_server->ten_env_proxy;

  // The http server thread is stopped completely, and we can now release
  // relevant resource to prevent memory leakages.
  delete http_server;

  ten_env.on_stop_done();
}

HTTP_METHOD parse_http_method(struct lws *wsi) {
  TEN_ASSERT(wsi, "Invalid argument.");

  if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI) != 0) {
    return HTTP_METHOD_GET;
  }

  if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI) != 0) {
    return HTTP_METHOD_POST;
  }

  if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI) != 0) {
    return HTTP_METHOD_OPTIONS;
  }

  if (lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI) != 0) {
    return HTTP_METHOD_PUT;
  }

  if (lws_hdr_total_length(wsi, WSI_TOKEN_DELETE_URI) != 0) {
    return HTTP_METHOD_DELETE;
  }

  if (lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI) != 0) {
    return HTTP_METHOD_PATCH;
  }

  return HTTP_METHOD_INVALID;
}

std::string get_http_method_string(HTTP_METHOD method) {
  switch (method) {
  case HTTP_METHOD_GET:
    return "HTTP_GET";
  case HTTP_METHOD_POST:
    return "HTTP_POST";
  case HTTP_METHOD_PUT:
    return "HTTP_PUT";
  case HTTP_METHOD_PATCH:
    return "HTTP_PATCH";
  case HTTP_METHOD_DELETE:
    return "HTTP_DELETE";
  case HTTP_METHOD_HEAD:
    return "HTTP_HEAD";
  case HTTP_METHOD_OPTIONS:
    return "HTTP_OPTIONS";
  default:
    assert(0 && "Invalid HTTP_METHOD.");
    return "INVALID";
  }
}

void remove_first(
    std::list<std::shared_ptr<http_transaction_data_t>> &list,
    const std::shared_ptr<http_transaction_data_t> &http_session_data) {
  ten_buf_t *resp_buf = http_session_data->resp_buf;
  assert(resp_buf && "Should not happen.");

  for (auto it = list.begin(); it != list.end(); ++it) {
    if ((*it)->resp_buf == resp_buf) {
      list.erase(it);
      break;
    }
  }
}

void send_ten_msg_with_req_body(
    const std::shared_ptr<http_transaction_data_t> &http_session_data);

void send_ten_msg_without_req_body(
    const std::shared_ptr<http_transaction_data_t> &http_session_data);

int event_callback(struct lws *wsi, enum lws_callback_reasons reason,
                   void *user, void *in, size_t len) {
  struct lws_context *lws_context = lws_get_context(wsi);
  assert(lws_context && "Invalid argument.");

  auto *http_server =
      static_cast<http_server_t *>(lws_context_user(lws_context));
  assert(http_server && "Should not happen.");

  auto *http_session_data =
      static_cast<std::shared_ptr<http_transaction_data_t> **>(user);

  switch (reason) {
  case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
    // Allocate all the resources for the transaction.
    if (*http_session_data == nullptr) {
      *http_session_data = new std::shared_ptr<http_transaction_data_t>(
          new http_transaction_data_t());

      (**http_session_data)->http_server = http_server;
      (**http_session_data)->lws_context = lws_context;
      (**http_session_data)->wsi = wsi;
      (**http_session_data)->req_buf =
          ten_buf_create_with_owned_data(DEFAULT_BUF_CAPACITY);
      (**http_session_data)->resp_buf =
          ten_buf_create_with_owned_data(DEFAULT_BUF_CAPACITY);
    }

    http_server->all_http_session_data.push_back(**http_session_data);
    break;

  case LWS_CALLBACK_HTTP: {
    assert(http_session_data && "Invalid argument.");

    if (http_server->react_ten_stopping_state > REACT_TEN_STOPPING_NOT_START) {
      // Do not handle more requests if we are about to close.
      int rc = lws_http_transaction_completed(wsi);
      TEN_LOGI("lws_http_transaction_completed: %d", rc);
      return -1;
    }

    HTTP_METHOD method = parse_http_method(wsi);
    assert(method != HTTP_METHOD_INVALID && "Should not happen.");

    (**http_session_data)->method = method;
    (**http_session_data)->url = static_cast<const char *>(in);

    switch (method) {
    case HTTP_METHOD_GET:
    case HTTP_METHOD_DELETE:
    case HTTP_METHOD_OPTIONS:
      // There is no HTTP request body, so handle the request directly
      // without waiting to receive the request body.
      send_ten_msg_without_req_body(**http_session_data);
      break;

    default:
      break;
    }

    return 0;
  }

  case LWS_CALLBACK_HTTP_BODY:
    assert(http_session_data && "Invalid argument.");
    assert(*http_session_data && "Invalid argument.");
    assert(**http_session_data && "Invalid argument.");

    // Add the received request data into the req_buf.
    ten_buf_push((**http_session_data)->req_buf, static_cast<uint8_t *>(in),
                 len);
    break;

  case LWS_CALLBACK_HTTP_BODY_COMPLETION:
    assert(http_session_data && "Invalid argument.");
    assert(*http_session_data && "Invalid argument.");
    assert(**http_session_data && "Invalid argument.");

    send_ten_msg_with_req_body(**http_session_data);
    break;

  case LWS_CALLBACK_HTTP_WRITEABLE:
    assert((**http_session_data)->wsi == wsi && "Should not happen");
    assert(http_session_data && "Invalid argument.");
    assert(*http_session_data && "Invalid argument.");
    assert(**http_session_data && "Invalid argument.");
    assert(ten_buf_get_content_size((**http_session_data)->resp_buf) > 0 &&
           "Should not happen.");

    // Return the response data of this session.
    return_response(**http_session_data, wsi);

    // The response is wrote out, so we mark the completion of the
    // transaction.
    if (lws_http_transaction_completed(wsi) > 0) {
      // A negative return value signals to libwebsockets that the current
      // connection needs to be closed.
      return -1;
    }

    break;

  case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
    // Remove all the resources relevant to the transaction.
    if (http_session_data != nullptr && *http_session_data != nullptr) {
      assert((**http_session_data)->wsi == wsi && "Should not happen");
      assert(*http_session_data && "Invalid argument.");
      assert(**http_session_data && "Invalid argument.");

      remove_first(http_server->all_http_session_data, **http_session_data);

      delete *http_session_data;
      *http_session_data = nullptr;
    }

    // Check if we have handled all the requests and the TEN is stopping.
    if (http_server->all_http_session_data.empty() &&
        http_server->react_ten_stopping_state == REACT_TEN_STOPPING) {
      http_server->react_ten_stopping_state.store(REACT_TEN_STOPPING_COMPLETED);

      // Notify the TEN world that it can proceed to stop the extension.
      http_server->ten_env_proxy->notify(
          [http_server](ten::ten_env_t &ten_env) {
            proceed_to_stop_http_extension(ten_env, http_server);
          });
    }
    break;

  case LWS_CALLBACK_EVENT_WAIT_CANCELLED: {
    // Triggered from the TEN world.

    bool has_more_resp = trigger_lws_write_out_timing(http_server);

    // Check if we have handled all the requests and the TEN is stopping.
    if (!has_more_resp &&
        http_server->react_ten_stopping_state == REACT_TEN_STOPPING) {
      http_server->react_ten_stopping_state.store(REACT_TEN_STOPPING_COMPLETED);

      http_server->ten_env_proxy->notify(
          [http_server](ten::ten_env_t &ten_env) {
            proceed_to_stop_http_extension(ten_env, http_server);
          });
    }
    break;
  }

  default:
    break;
  }

  // Returning 0 indicates that the event was handled successfully and there
  // were no errors. This is the most common return value for many events,
  // signaling that everything is proceeding normally.
  return 0;
}

const struct lws_protocols protocols[] = {
    {
        "http_server",
        event_callback,
        sizeof(std::shared_ptr<http_transaction_data_t> *),
        0,
        0,
        nullptr,
        0,
    },
    {nullptr, nullptr, 0, 0, 0, nullptr, 0}};

struct lws_context *lws_context_new(const char *server_url, int server_port,
                                    http_server_t *http_server) {
  struct lws_context_creation_info info{};
  memset(&info, 0, sizeof info);

  info.protocols = protocols;
  info.server_string = server_url;
  info.port = server_port;
  info.gid = -1;
  info.uid = -1;
  info.connect_timeout_secs = 30;
  info.keepalive_timeout = 60;
  info.user = http_server;

  struct lws_context *context = lws_create_context(&info);
  assert(context && "Failed to create libwebsockets context");

  return context;
}

http_server_t *create_http_server(const char *server_url, int server_port) {
  auto *http_server = new http_server_t();
  assert(http_server && "Failed to allocate memory for http_server_t");

  struct lws_context *lws_context =
      lws_context_new(server_url, server_port, http_server);
  assert(lws_context && "Failed to create libwebsockets context");

  http_server->lws_context = lws_context;
  http_server->react_ten_stopping_state.store(REACT_TEN_STOPPING_NOT_START);

  return http_server;
}

std::thread create_http_server_thread(http_server_t *http_server) {
  std::thread http_server_thread = std::thread([http_server]() {
    int n = 0;
    while (n >= 0 && (http_server->react_ten_stopping_state <
                      REACT_TEN_STOPPING_COMPLETED)) {
      n = lws_service(http_server->lws_context, 0);
    }

    lws_context_destroy(http_server->lws_context);
  });

  return http_server_thread;
}

class http_server_extension_t : public ten::extension_t {
 public:
  explicit http_server_extension_t(const char *name) : extension_t(name) {}

  void on_start(ten::ten_env_t &ten_env) override {
    int server_port = DEFAULT_SERVER_PORT;

    auto port = ten_env.get_property_int64("server_port");
    if (port > 0) {
      server_port = static_cast<int>(port);
    }

    http_server = create_http_server(DEFAULT_SERVER_URL, server_port);

    // Create a TEN proxy to be used by the http server.
    http_server->ten_env_proxy = ten::ten_env_proxy_t::create(ten_env);

    http_server->http_server_thread = create_http_server_thread(http_server);

    ten_env.on_start_done();
  }

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Receive cmd from ten graph.
  }

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    // Receive data from ten graph.
  }

  void on_audio_frame(ten::ten_env_t &ten_env,
                      std::unique_ptr<ten::audio_frame_t> frame) override {
    // Receive audio frame from ten graph.
  }

  void on_video_frame(ten::ten_env_t &ten_env,
                      std::unique_ptr<ten::video_frame_t> frame) override {
    // Receive video frame from ten graph.
  }

  void on_stop(ten::ten_env_t & /*ten*/) override {
    // Extension stop.

    is_stopping = true;
    http_server->react_ten_stopping_state.store(REACT_TEN_STOPPING);
    lws_cancel_service(http_server->lws_context);
  }

 private:
  friend void send_ten_msg_without_req_body(
      const std::shared_ptr<http_transaction_data_t> &http_session_data);

  friend void send_ten_msg_with_req_body(
      const std::shared_ptr<http_transaction_data_t> &http_session_data);

  http_server_t *http_server{nullptr};
  bool is_stopping{false};
};

void send_ten_msg_with_req_body(
    const std::shared_ptr<http_transaction_data_t> &http_session_data) {
  // We are _not_ in the TEN threads, so we need to use ten_env_proxy.
  http_session_data->http_server->ten_env_proxy->notify([http_session_data](
                                                            ten::ten_env_t
                                                                &ten_env) {
    // Create a TEN command from the request.

    // Parse the received request data and create a command from it
    // according to the request content.
    auto cmd_json = nlohmann::json::parse(
        std::string(reinterpret_cast<char *>(http_session_data->req_buf->data),
                    ten_buf_get_content_size(http_session_data->req_buf))
            .c_str());

    std::string method = get_http_method_string(http_session_data->method);

    std::unique_ptr<ten::cmd_t> cmd = nullptr;

    if (cmd_json.contains("ten")) {
      if (cmd_json["ten"].contains("type")) {
        // Should be a TEN internal command.

        if (cmd_json["ten"]["type"] == "close_app") {
          cmd = ten::cmd_close_app_t::create();

          // Set the destination of the command to the localhost.
          cmd->set_dest("localhost", nullptr, nullptr, nullptr);
        } else {
          assert(0 && "Handle more internal command types.");
        }
      } else if (cmd_json["ten"].contains("name")) {
        // Should be a custom command.

        cmd = ten::cmd_t::create(
            cmd_json["ten"]["name"].get<std::string>().c_str());

        // If the cmd_json contains "dest", it means the command should be sent
        // to a specific extension.
        if (cmd_json["ten"].contains("dest")) {
          auto dest = cmd_json["ten"]["dest"];

          try {
            cmd->set_dest(dest["app"].get<std::string>().c_str(),
                          dest["graph"].get<std::string>().c_str(),
                          dest["extension_group"].get<std::string>().c_str(),
                          dest["extension"].get<std::string>().c_str());
          } catch (const std::exception &e) {
            TEN_LOGW("Failed to set the destination of the command: %s",
                     e.what());
          }
        }
      }
    }

    if (cmd == nullptr) {
      // Use the method as the command name by default.
      cmd = ten::cmd_t::create(method.c_str());
    }

    cmd_json["method"] =
        get_http_method_string(http_session_data->method).c_str();
    cmd_json["url"] = http_session_data->url.c_str();

    // Parse the full content of the request and set it to the
    // command.
    cmd->set_property_from_json(nullptr, cmd_json.dump().c_str());

    // Send out the command to the TEN runtime.
    ten_env.send_cmd(
        std::move(cmd),
        [http_session_data](ten::ten_env_t &ten_env,
                            std::unique_ptr<ten::cmd_result_t> cmd_result,
                            ten::error_t *err) {
          if (err != nullptr) {
            prepare_response_data_from_ten_world(
                http_session_data, "The command is not supported. err:" +
                                       std::string(err->error_message()));
            return;
          }

          auto *ext = static_cast<http_server_extension_t *>(
              ten::ten_env_internal_accessor_t::get_attached_target(ten_env));
          assert(ext && "Failed to get the attached extension.");

          if (!ext->is_stopping) {
            // When stopping, do not push more data into libws thread.
            // Libws world would clean up itself.
            prepare_response_data_from_ten_world(
                http_session_data, cmd_result->get_property_to_json("detail"));
          }
        });
  });
}

void send_ten_msg_without_req_body(
    const std::shared_ptr<http_transaction_data_t> &http_session_data) {
  // We are _not_ in the TEN threads, so we need to use ten_env_proxy.
  http_session_data->http_server->ten_env_proxy->notify(
      [http_session_data](ten::ten_env_t &ten_env) {
        // Create a TEN command from the request.

        std::string method = get_http_method_string(http_session_data->method);
        auto cmd = ten::cmd_t::create(method.c_str());
        cmd->set_property("method", method);
        cmd->set_property("url", http_session_data->url);

        // Send out the command to the TEN runtime.
        ten_env.send_cmd(
            std::move(cmd),
            [http_session_data](ten::ten_env_t &ten_env,
                                std::unique_ptr<ten::cmd_result_t> cmd_result,
                                ten::error_t *err) {
              if (err != nullptr) {
                prepare_response_data_from_ten_world(
                    http_session_data, "The command is not supported. err:" +
                                           std::string(err->error_message()));
                return;
              }

              auto *ext = static_cast<http_server_extension_t *>(
                  ten::ten_env_internal_accessor_t::get_attached_target(
                      ten_env));
              assert(ext && "Failed to get the attached extension.");

              if (!ext->is_stopping) {
                // When stopping, do not push more data into libws thread. Libws
                // world would clean up itself.
                prepare_response_data_from_ten_world(
                    http_session_data,
                    cmd_result->get_property_to_json("detail"));
              }
            });
      });
}

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(simple_http_server_cpp,
                                    http_server_extension_t);

}  // namespace
