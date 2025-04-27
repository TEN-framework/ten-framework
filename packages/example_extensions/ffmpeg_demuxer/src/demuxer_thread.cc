//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "demuxer_thread.h"

#include <libavutil/rational.h>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "demuxer.h"
#include "ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lang/cpp/lib/value.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

namespace ten {
namespace ffmpeg_extension {

demuxer_thread_t::demuxer_thread_t(ten::ten_env_proxy_t *ten_env_proxy,
                                   std::unique_ptr<ten::cmd_t> start_cmd,
                                   extension_t *extension,
                                   std::string &input_stream_loc)
    : ten_env_proxy_(ten_env_proxy),
      extension_(extension),
      stop_(false),
      demuxer(nullptr),
      demuxer_thread(nullptr),
      event_for_start_demuxing(ten_event_create(0, 0)),
      input_stream_loc_(input_stream_loc),
      start_cmd_(std::move(start_cmd)) {
  TEN_ASSERT(extension, "Invalid argument.");
}

demuxer_thread_t::~demuxer_thread_t() {
  if (demuxer != nullptr) {
    delete demuxer;
    demuxer = nullptr;
  }

  ten_event_destroy(event_for_start_demuxing);

  delete ten_env_proxy_;
}

void *demuxer_thread_main(void *self_) {
  TEN_LOGD("Demuxer thread is started.");

  auto *demuxer_thread = reinterpret_cast<demuxer_thread_t *>(self_);
  TEN_ASSERT(demuxer_thread, "Invalid argument.");

  if (!demuxer_thread->create_demuxer()) {
    TEN_LOGW("Failed to create demuxer, stop the demuxer thread.");

    demuxer_thread->reply_to_start_cmd(false);
    return nullptr;
  }

  TEN_ASSERT(demuxer_thread->demuxer, "Demuxer should have been created.");

  // Notify that the demuxer has been created.
  demuxer_thread->reply_to_start_cmd(true);

  // The demuxter thread will be blocked until receiving start signal.
  demuxer_thread->wait_to_start_demuxing();

  // Starts the demuxer loop.
  TEN_LOGD("Start the demuxer thread loop.");

  DECODE_STATUS status = DECODE_STATUS_SUCCESS;
  while (!demuxer_thread->is_stopped() && status == DECODE_STATUS_SUCCESS) {
    // Decode next input frame.
    status = demuxer_thread->demuxer->decode_next_packet();

    switch (status) {
      case DECODE_STATUS_EOF:
        TEN_LOGD("Input stream is ended, stop the demuxer thread normally.");

        // Send EOF frame, so that the subsequent stages could know this fact.
        demuxer_thread->send_video_eof();
        demuxer_thread->send_audio_eof();
        break;

      case DECODE_STATUS_ERROR:
        TEN_LOGW("Something bad happened, stop the demuxer thread abruptly.");
        break;

      default:
        break;
    }
  }

  demuxer_thread->notify_completed(status == DECODE_STATUS_EOF);

  TEN_LOGD("Demuxer thread is stopped.");

  return nullptr;
}

void demuxer_thread_t::notify_completed(bool success) {
  ten_env_proxy_->notify([this, success](ten::ten_env_t &ten_env) {
    auto cmd = ten::cmd_t::create("complete");
    cmd->set_property("input_stream", input_stream_loc_);
    cmd->set_property("success", success);
    ten_env.send_cmd(std::move(cmd));
  });
}

void demuxer_thread_t::start() {
  demuxer_thread = ten_thread_create(nullptr, demuxer_thread_main, this);
  TEN_ASSERT(demuxer_thread, "Invalid argument.");
}

void demuxer_thread_t::start_demuxing() {
  ten_event_set(event_for_start_demuxing);
}

void demuxer_thread_t::wait_to_start_demuxing() {
  ten_event_wait(event_for_start_demuxing, -1);
}

void demuxer_thread_t::wait_for_stop_completed() {
  int rc = ten_thread_join(demuxer_thread, -1);
  TEN_ASSERT(!rc, "Invalid argument.");

  TEN_LOGD("Demuxer thread has been reclaimed.");
}

bool demuxer_thread_t::create_demuxer() {
  demuxer = new demuxer_t(ten_env_proxy_, this);
  TEN_ASSERT(demuxer, "Invalid argument.");

  return demuxer->open_input_stream(input_stream_loc_);
}

// Called from the demuxer thread.
void demuxer_thread_t::send_video_eof() {
  auto frame = ten::video_frame_t::create("video_frame");
  frame->set_eof(true);

  auto frame_shared =
      std::make_shared<std::unique_ptr<ten::video_frame_t>>(std::move(frame));

  ten_env_proxy_->notify([frame_shared](ten::ten_env_t &ten_env) {
    ten_env.send_video_frame(std::move(*frame_shared));
  });
}

// Called from the demuxer thread.
void demuxer_thread_t::send_audio_eof() {
  auto frame = ten::audio_frame_t::create("audio_frame");
  frame->set_eof(true);

  auto frame_shared =
      std::make_shared<std::unique_ptr<ten::audio_frame_t>>(std::move(frame));

  ten_env_proxy_->notify([frame_shared](ten::ten_env_t &ten_env) {
    ten_env.send_audio_frame(std::move(*frame_shared));
  });
}

namespace {

double rational_to_double(const AVRational &r) {
  return r.num / static_cast<double>(r.den);
}

}  // namespace

void demuxer_thread_t::reply_to_start_cmd(bool success) {
  if (!success || demuxer == nullptr) {
    ten_env_proxy_->notify([this](ten::ten_env_t &ten_env) {
      auto cmd_result =
          ten::cmd_result_t::create(TEN_STATUS_CODE_ERROR, *start_cmd_);
      cmd_result->set_property("detail", "fail to prepare demuxer.");
      ten_env.return_result(std::move(cmd_result));
    });
    return;
  }

  auto cmd_result = ten::cmd_result_t::create(TEN_STATUS_CODE_OK, *start_cmd_);
  cmd_result->set_property("detail", "The demuxer has been started.");

  // Demuxer video settings.
  cmd_result->set_property("frame_rate_num", demuxer->video_frame_rate().num);
  cmd_result->set_property("frame_rate_den", demuxer->video_frame_rate().den);
  cmd_result->set_property("frame_rate_d",
                           rational_to_double(demuxer->video_frame_rate()));

  cmd_result->set_property("video_time_base_num",
                           demuxer->video_time_base().num);
  cmd_result->set_property("video_time_base_den",
                           demuxer->video_time_base().den);
  cmd_result->set_property("video_time_base_d",
                           rational_to_double(demuxer->video_time_base()));

  cmd_result->set_property("width", demuxer->video_width());
  cmd_result->set_property("height", demuxer->video_height());
  cmd_result->set_property("bit_rate", demuxer->video_bit_rate());
  cmd_result->set_property("num_of_frames", demuxer->number_of_video_frames());

  // Demuxer audio settings.
  cmd_result->set_property("audio_sample_rate", demuxer->audio_sample_rate);
  cmd_result->set_property("audio_channel_layout_mask",
                           demuxer->audio_channel_layout_mask);
  cmd_result->set_property("audio_num_of_channels",
                           demuxer->audio_num_of_channels);
  cmd_result->set_property("audio_time_base_num",
                           demuxer->audio_time_base().num);
  cmd_result->set_property("audio_time_base_den",
                           demuxer->audio_time_base().den);
  cmd_result->set_property("audio_time_base_d",
                           rational_to_double(demuxer->audio_time_base()));

  auto result_shared = std::make_shared<std::unique_ptr<ten::cmd_result_t>>(
      std::move(cmd_result));
  ten_env_proxy_->notify([result_shared](ten::ten_env_t &ten_env) {
    ten_env.return_result(std::move(*result_shared));
  });
}

}  // namespace ffmpeg_extension
}  // namespace ten
