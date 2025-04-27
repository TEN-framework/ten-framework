//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "muxer_thread.h"

#include <list>
#include <utility>

#include "muxer.h"
#include "ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/event.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/log/log.h"

namespace ten {
namespace ffmpeg_extension {

muxer_thread_t::muxer_thread_t(ten::ten_env_proxy_t *ten_env_proxy,
                               demuxer_settings_t &settings,
                               std::string output_stream)
    : muxer_thread(nullptr),
      muxer_thread_is_started(ten_event_create(0, 0)),
      is_stop(0),
      muxer(nullptr),
      out_lock(ten_mutex_create()),
      out_available(ten_event_create(0, 1)),
      settings(settings),
      output_stream(std::move(output_stream)),
      audio_eof(false),
      video_eof(false),
      ten_env_proxy(ten_env_proxy) {}

muxer_thread_t::~muxer_thread_t() {
  ten_mutex_destroy(out_lock);
  ten_event_destroy(out_available);

  out_audios.clear();
  out_images.clear();

  // Clear muxer relevant resources.
  ten_event_destroy(muxer_thread_is_started);

  delete muxer;
  delete ten_env_proxy;

  TEN_LOGD("All the muxer resources have been cleaned.");
}

void muxer_thread_t::wait_for_the_first_av_frame() {
  while ((ten_atomic_load(&is_stop) == 0) && out_audios.empty() &&
         out_images.empty()) {
    int rc = ten_mutex_lock(out_lock);
    TEN_ASSERT(!rc, "Should not happen.");

    if (out_audios.empty() && out_images.empty()) {
      rc = ten_mutex_unlock(out_lock);
      TEN_ASSERT(!rc, "Should not happen.");

      TEN_LOGD("No further frames need to be muxed, wait...");

      rc = ten_event_wait(out_available, -1);
      TEN_ASSERT(!rc, "Should not happen.");

      continue;
    }

    rc = ten_mutex_unlock(out_lock);
    TEN_ASSERT(!rc, "Should not happen.");
  }
}

void *muxer_thread_main(void *self_) {
  TEN_LOGD("Muxer thread is started.");

  auto *self = static_cast<muxer_thread_t *>(self_);
  TEN_ASSERT(self, "Invalid argument.");

  if (ten_atomic_load(&self->is_stop) != 0) {
    // Muxer thread has already been triggered to stop.
    return nullptr;
  }

  self->create_muxer();

  ten_event_set(self->muxer_thread_is_started);

  // Wait for the first frame to come before starting, to avoid meaningless CPU
  // resource wastage.
  self->wait_for_the_first_av_frame();

  TEN_LOGD("Starting to mux...");

  const int64_t start_time = ten_current_time_ms();
  int64_t sleep_overhead = 0;
  bool status = true;

  std::list<std::unique_ptr<ten::audio_frame_t>> out_audios;
  std::list<std::unique_ptr<ten::video_frame_t>> out_images;

  while ((ten_atomic_load(&self->is_stop) == 0 || !self->out_audios.empty() ||
          !self->out_images.empty()) &&
         status && !(self->audio_eof && self->video_eof)) {
    out_audios.clear();
    out_images.clear();

    {
      // Get the received audio + video frames at once.

      int rc = ten_mutex_lock(self->out_lock);
      TEN_ASSERT(!rc, "Should not happen.");

      out_audios.swap(self->out_audios);
      out_images.swap(self->out_images);

      rc = ten_mutex_unlock(self->out_lock);
      TEN_ASSERT(!rc, "Should not happen.");
    }

    for (auto &out_audio : out_audios) {
      auto encode_status =
          self->muxer->encode_audio_frame(std::move(out_audio));
      status = encode_status != ENCODE_STATUS_ERROR;
      TEN_ASSERT(status, "Should not happen.");

      if (encode_status == ENCODE_STATUS_EOF) {
        self->audio_eof = true;
      }
    }

    for (auto &out_image : out_images) {
      auto encode_status =
          self->muxer->encode_video_frame(std::move(out_image));
      status = encode_status != ENCODE_STATUS_ERROR;
      TEN_ASSERT(status, "Should not happen.");

      if (encode_status == ENCODE_STATUS_EOF) {
        self->video_eof = true;
      }

      if (status) {
        int64_t now = ten_current_time_ms();
        int64_t expected_time = start_time + self->muxer->next_video_timing();
        int64_t sleep_time = expected_time - now - sleep_overhead;
        if (sleep_time > 0) {
          ten_sleep_ms(sleep_time);

          // 'sleep' would cause thread switching, and thread switching would
          // have time overhead. Based on our frame rate, this time cannot be
          // ignored, so we need to consider it when we muxing next frame.
          sleep_overhead = ten_current_time_ms() - now - sleep_time;
        } else {
          sleep_overhead = 0;
        }
      }
    }
  }

  out_audios.clear();
  out_images.clear();

  self->notify_completed(status);

  TEN_LOGD("Muxer thread is stopped.");
  return nullptr;
}

void muxer_thread_t::start() {
  muxer_thread = ten_thread_create(nullptr, muxer_thread_main, this);
  TEN_ASSERT(muxer_thread, "Invalid argument.");
}

void muxer_thread_t::wait_for_start() {
  ten_event_wait(muxer_thread_is_started, -1);
}

void muxer_thread_t::stop() {
  TEN_LOGD("Signal muxer thread to close.");

  ten_atomic_store(&is_stop, 1);

  // Kick muxer thread in case it is waiting for the first av frame.
  ten_event_set(out_available);
}

void muxer_thread_t::notify_completed(bool success) {
  auto cmd = ten::cmd_t::create("complete");
  cmd->set_property("input_stream", output_stream);
  cmd->set_property("success", success);

  auto cmd_shared =
      std::make_shared<std::unique_ptr<ten::cmd_t>>(std::move(cmd));

  ten_env_proxy->notify([cmd_shared](ten::ten_env_t &ten_env) {
    ten_env.send_cmd(std::move(*cmd_shared));
  });
}

void muxer_thread_t::wait_for_stop() {
  int rc = ten_thread_join(muxer_thread, -1);
  TEN_ASSERT(!rc, "Invalid argument.");

  TEN_LOGD("Muxer thread has been reclaimed.");
}

void muxer_thread_t::create_muxer() {
  muxer = new muxer_t();
  TEN_ASSERT(muxer, "Invalid argument.");

  muxer->src_video_width = settings.src_video_width;
  muxer->src_video_height = settings.src_video_height;
  muxer->src_video_number_of_frames = settings.src_video_number_of_frames;
  muxer->src_video_bit_rate = settings.src_video_bit_rate;
  muxer->src_video_frame_rate = settings.src_video_frame_rate;
  muxer->src_video_time_base = settings.src_video_time_base;

  muxer->src_audio_sample_rate = settings.src_audio_sample_rate;
  muxer->src_audio_time_base = settings.src_audio_time_base;
  muxer->src_audio_channel_layout_mask = settings.src_audio_channel_layout_mask;

  muxer->open(output_stream, false);
}

// Put received audio frame from TEN world to FFmpeg.
void muxer_thread_t::on_ten_audio_frame(
    std::unique_ptr<ten::audio_frame_t> frame) {
  int rc = ten_mutex_lock(out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  out_audios.push_back(std::move(frame));

  rc = ten_mutex_unlock(out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_event_set(out_available);
}

// Put received video frame from TEN world to FFmpeg.
void muxer_thread_t::on_ten_video_frame(
    std::unique_ptr<ten::video_frame_t> frame) {
  int rc = ten_mutex_lock(out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  out_images.push_back(std::move(frame));

  rc = ten_mutex_unlock(out_lock);
  TEN_ASSERT(!rc, "Should not happen.");

  ten_event_set(out_available);
}

}  // namespace ffmpeg_extension
}  // namespace ten
