//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "ten_runtime/binding/cpp/ten.h"

namespace {

class simple_echo_extension_t : public ten::extension_t {
 public:
  explicit simple_echo_extension_t(const char *name) : ten::extension_t(name) {}

  void on_cmd(ten::ten_env_t &ten_env,
              std::unique_ptr<ten::cmd_t> cmd) override {
    // Parse the command and return a new command with the same name but with a
    // suffix ", too" added to it.

    std::string cmd_name = cmd->get_name();

    auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *cmd);
    cmd_result->set_property("detail", cmd_name + ", too");

    ten_env.return_result(std::move(cmd_result));
  }

  void on_data(ten::ten_env_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    // Receive data from ten graph.
    auto buf = data->get_buf();

    auto new_data = ten::data_t::create(data->get_name().c_str());
    new_data->alloc_buf(buf.size());
    auto new_buf = new_data->lock_buf();
    memcpy(new_buf.data(), buf.data(), buf.size());
    new_data->unlock_buf(new_buf);

    ten_env.send_data(std::move(new_data));
  }

  void on_video_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::video_frame_t> video_frame) override {
    // Receive video frame from ten graph.
    auto buf = video_frame->lock_buf();

    auto new_video_frame =
        ten::video_frame_t::create(video_frame->get_name().c_str());
    new_video_frame->alloc_buf(buf.size());
    auto new_buf = new_video_frame->lock_buf();
    memcpy(new_buf.data(), buf.data(), buf.size());
    new_video_frame->unlock_buf(new_buf);

    video_frame->unlock_buf(buf);

    new_video_frame->set_width(video_frame->get_width());
    new_video_frame->set_height(video_frame->get_height());
    new_video_frame->set_pixel_fmt(video_frame->get_pixel_fmt());
    new_video_frame->set_timestamp(video_frame->get_timestamp());
    new_video_frame->set_eof(video_frame->is_eof());

    ten_env.send_video_frame(std::move(new_video_frame));
  }

  void on_audio_frame(
      ten::ten_env_t &ten_env,
      std::unique_ptr<ten::audio_frame_t> audio_frame) override {
    // Receive audio frame from ten graph.
    auto buf = audio_frame->lock_buf();

    auto new_audio_frame =
        ten::audio_frame_t::create(audio_frame->get_name().c_str());
    new_audio_frame->alloc_buf(buf.size());
    auto new_buf = new_audio_frame->lock_buf();
    memcpy(new_buf.data(), buf.data(), buf.size());
    new_audio_frame->unlock_buf(new_buf);

    audio_frame->unlock_buf(buf);

    new_audio_frame->set_sample_rate(audio_frame->get_sample_rate());
    new_audio_frame->set_bytes_per_sample(audio_frame->get_bytes_per_sample());
    new_audio_frame->set_samples_per_channel(
        audio_frame->get_samples_per_channel());
    new_audio_frame->set_channel_layout(audio_frame->get_channel_layout());
    new_audio_frame->set_number_of_channels(
        audio_frame->get_number_of_channels());
    new_audio_frame->set_timestamp(audio_frame->get_timestamp());
    new_audio_frame->set_eof(audio_frame->is_eof());
    new_audio_frame->set_data_fmt(audio_frame->get_data_fmt());
    new_audio_frame->set_line_size(audio_frame->get_line_size());

    ten_env.send_audio_frame(std::move(new_audio_frame));
  }
};

}  // namespace

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(simple_echo_cpp, simple_echo_extension_t);
