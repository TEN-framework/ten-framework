//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include <memory>

#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/cpp/detail/msg/audio_frame.h"
#include "ten_runtime/binding/cpp/detail/msg/cmd/cmd.h"
#include "ten_runtime/binding/cpp/detail/msg/data.h"
#include "ten_runtime/binding/cpp/detail/msg/video_frame.h"
#include "ten_runtime/binding/cpp/detail/test/env_tester.h"
#include "ten_runtime/test/extension_tester.h"
#include "ten_utils/macro/check.h"

namespace ten {

class extension_tester_internal_accessor_t;

class extension_tester_t {
 public:
  virtual ~extension_tester_t() {
    TEN_ASSERT(c_extension_tester, "Should not happen.");
    ten_extension_tester_destroy(c_extension_tester);

    TEN_ASSERT(cpp_ten_env_tester, "Should not happen.");
    delete cpp_ten_env_tester;
  }

  // @{
  extension_tester_t(const extension_tester_t &) = delete;
  extension_tester_t(extension_tester_t &&) = delete;
  extension_tester_t &operator=(const extension_tester_t &) = delete;
  extension_tester_t &operator=(const extension_tester_t &&) = delete;
  // @}

  void set_test_mode_single(const char *addon_name,
                            const char *property_json_str = nullptr) {
    TEN_ASSERT(addon_name, "Invalid argument.");
    ten_extension_tester_set_test_mode_single(c_extension_tester, addon_name,
                                              property_json_str);
  }

  void set_test_mode_graph(const char *graph_json) {
    TEN_ASSERT(graph_json, "Invalid argument.");
    ten_extension_tester_set_test_mode_graph(c_extension_tester, graph_json);
  }

  bool run(error_t *err = nullptr) {
    TEN_ASSERT(c_extension_tester, "Should not happen.");
    return ten_extension_tester_run(c_extension_tester);
  }

 protected:
  explicit extension_tester_t()
      : c_extension_tester(::ten_extension_tester_create(
            nullptr,
            reinterpret_cast<ten_extension_tester_on_start_func_t>(
                &proxy_on_start),
            nullptr, nullptr,
            reinterpret_cast<ten_extension_tester_on_cmd_func_t>(&proxy_on_cmd),
            reinterpret_cast<ten_extension_tester_on_data_func_t>(
                &proxy_on_data),
            reinterpret_cast<ten_extension_tester_on_audio_frame_func_t>(
                &proxy_on_audio_frame),
            reinterpret_cast<ten_extension_tester_on_video_frame_func_t>(
                &proxy_on_video_frame))) {
    TEN_ASSERT(c_extension_tester, "Should not happen.");

    ten_binding_handle_set_me_in_target_lang(
        reinterpret_cast<ten_binding_handle_t *>(c_extension_tester),
        static_cast<void *>(this));

    cpp_ten_env_tester = new ten_env_tester_t(
        ten_extension_tester_get_ten_env_tester(c_extension_tester));
    TEN_ASSERT(cpp_ten_env_tester, "Should not happen.");
  }

  virtual void on_start(ten_env_tester_t &ten_env_tester) {
    ten_env_tester.on_start_done();
  }

  virtual void on_cmd(ten_env_tester_t &ten_env_tester,
                      std::unique_ptr<cmd_t> cmd) {}

  virtual void on_data(ten_env_tester_t &ten_env_tester,
                       std::unique_ptr<data_t> data) {}

  virtual void on_audio_frame(ten_env_tester_t &ten_env_tester,
                              std::unique_ptr<audio_frame_t> audio_frame) {}

  virtual void on_video_frame(ten_env_tester_t &ten_env_tester,
                              std::unique_ptr<video_frame_t> video_frame) {}

 private:
  friend class extension_tester_internal_accessor_t;

  void invoke_cpp_extension_tester_on_start(
      ten_env_tester_t &cpp_ten_env_tester) {
    on_start(cpp_ten_env_tester);
  }

  static void proxy_on_start(ten_extension_tester_t *tester,
                             ::ten_env_tester_t *c_ten_env_tester) {
    TEN_ASSERT(tester && c_ten_env_tester, "Should not happen.");

    auto *cpp_extension_tester = static_cast<extension_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(tester)));
    auto *cpp_ten_env_tester = static_cast<ten_env_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(c_ten_env_tester)));

    cpp_extension_tester->invoke_cpp_extension_tester_on_start(
        *cpp_ten_env_tester);
  }

  void invoke_cpp_extension_on_cmd(ten_env_tester_t &cpp_ten_env_tester,
                                   std::unique_ptr<cmd_t> cmd) {
    on_cmd(cpp_ten_env_tester, std::move(cmd));
  }

  static void proxy_on_cmd(ten_extension_tester_t *extension_tester,
                           ::ten_env_tester_t *c_ten_env_tester,
                           ten_shared_ptr_t *cmd) {
    TEN_ASSERT(extension_tester && c_ten_env_tester && cmd,
               "Should not happen.");

    auto *cpp_extension_tester = static_cast<extension_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(extension_tester)));
    auto *cpp_ten_env_tester = static_cast<ten_env_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(c_ten_env_tester)));

    // Clone a C shared_ptr to be owned by the C++ instance.
    cmd = ten_shared_ptr_clone(cmd);

    auto *cpp_cmd_ptr = new cmd_t(cmd);
    auto cpp_cmd_unique_ptr = std::unique_ptr<cmd_t>(cpp_cmd_ptr);

    cpp_extension_tester->invoke_cpp_extension_on_cmd(
        *cpp_ten_env_tester, std::move(cpp_cmd_unique_ptr));
  }

  void invoke_cpp_extension_on_data(ten_env_tester_t &cpp_ten_env_tester,
                                    std::unique_ptr<data_t> data) {
    on_data(cpp_ten_env_tester, std::move(data));
  }

  static void proxy_on_data(ten_extension_tester_t *extension_tester,
                            ::ten_env_tester_t *c_ten_env_tester,
                            ten_shared_ptr_t *data) {
    TEN_ASSERT(extension_tester && c_ten_env_tester && data,
               "Should not happen.");

    auto *cpp_extension_tester = static_cast<extension_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(extension_tester)));
    auto *cpp_ten_env_tester = static_cast<ten_env_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(c_ten_env_tester)));

    // Clone a C shared_ptr to be owned by the C++ instance.
    data = ten_shared_ptr_clone(data);

    auto *cpp_data_ptr = new data_t(data);
    auto cpp_data_unique_ptr = std::unique_ptr<data_t>(cpp_data_ptr);

    cpp_extension_tester->invoke_cpp_extension_on_data(
        *cpp_ten_env_tester, std::move(cpp_data_unique_ptr));
  }

  void invoke_cpp_extension_on_audio_frame(
      ten_env_tester_t &cpp_ten_env_tester,
      std::unique_ptr<audio_frame_t> audio_frame) {
    on_audio_frame(cpp_ten_env_tester, std::move(audio_frame));
  }

  static void proxy_on_audio_frame(ten_extension_tester_t *extension_tester,
                                   ::ten_env_tester_t *c_ten_env_tester,
                                   ten_shared_ptr_t *audio_frame) {
    TEN_ASSERT(extension_tester && c_ten_env_tester && audio_frame,
               "Should not happen.");

    auto *cpp_extension_tester = static_cast<extension_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(extension_tester)));
    auto *cpp_ten_env_tester = static_cast<ten_env_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(c_ten_env_tester)));

    // Clone a C shared_ptr to be owned by the C++ instance.
    audio_frame = ten_shared_ptr_clone(audio_frame);

    auto *cpp_audio_frame_ptr = new audio_frame_t(audio_frame);
    auto cpp_audio_frame_unique_ptr =
        std::unique_ptr<audio_frame_t>(cpp_audio_frame_ptr);

    cpp_extension_tester->invoke_cpp_extension_on_audio_frame(
        *cpp_ten_env_tester, std::move(cpp_audio_frame_unique_ptr));
  }

  void invoke_cpp_extension_on_video_frame(
      ten_env_tester_t &cpp_ten_env_tester,
      std::unique_ptr<video_frame_t> video_frame) {
    on_video_frame(cpp_ten_env_tester, std::move(video_frame));
  }

  static void proxy_on_video_frame(ten_extension_tester_t *extension_tester,
                                   ::ten_env_tester_t *c_ten_env_tester,
                                   ten_shared_ptr_t *video_frame) {
    TEN_ASSERT(extension_tester && c_ten_env_tester && video_frame,
               "Should not happen.");

    auto *cpp_extension_tester = static_cast<extension_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(extension_tester)));
    auto *cpp_ten_env_tester = static_cast<ten_env_tester_t *>(
        ten_binding_handle_get_me_in_target_lang(
            reinterpret_cast<ten_binding_handle_t *>(c_ten_env_tester)));

    // Clone a C shared_ptr to be owned by the C++ instance.
    video_frame = ten_shared_ptr_clone(video_frame);

    auto *cpp_video_frame_ptr = new video_frame_t(video_frame);
    auto cpp_video_frame_unique_ptr =
        std::unique_ptr<video_frame_t>(cpp_video_frame_ptr);

    cpp_extension_tester->invoke_cpp_extension_on_video_frame(
        *cpp_ten_env_tester, std::move(cpp_video_frame_unique_ptr));
  }

  ::ten_extension_tester_t *c_extension_tester;
  ten_env_tester_t *cpp_ten_env_tester;
};

}  // namespace ten
