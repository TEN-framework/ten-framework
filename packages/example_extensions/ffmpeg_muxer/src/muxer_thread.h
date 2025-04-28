//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include <cassert>
#include <cstdint>
#include <list>
#include <string>

#include "muxer.h"
#include "ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/atomic.h"
#include "ten_utils/lib/mutex.h"
#include "ten_utils/lib/thread.h"

namespace ten {
namespace ffmpeg_extension {

struct demuxer_settings_t {
  // @{
  // Source video settings.
  int src_video_width;
  int src_video_height;
  int64_t src_video_bit_rate;
  int64_t src_video_number_of_frames;
  AVRational src_video_frame_rate;
  AVRational src_video_time_base;
  // @}

  // @{
  // Source audio settings.
  int src_audio_sample_rate;
  AVRational src_audio_time_base;
  uint64_t src_audio_channel_layout_mask;
  // @}
};

class muxer_thread_t {
 public:
  explicit muxer_thread_t(ten::ten_env_proxy_t *, demuxer_settings_t &,
                          std::string);
  ~muxer_thread_t();

  muxer_thread_t(const muxer_thread_t &other) = delete;
  muxer_thread_t(muxer_thread_t &&other) = delete;

  muxer_thread_t &operator=(const muxer_thread_t &other) = delete;
  muxer_thread_t &operator=(muxer_thread_t &&other) = delete;

  void start();
  void wait_for_start();

  void stop();
  void wait_for_stop();

  void on_ten_audio_frame(std::unique_ptr<ten::audio_frame_t> frame);
  void on_ten_video_frame(std::unique_ptr<ten::video_frame_t> frame);

 private:
  friend void *muxer_thread_main(void *self);

  void create_muxer();
  void wait_for_the_first_av_frame();
  void notify_completed(bool success = true);

  ten_thread_t *muxer_thread;
  ten_event_t *muxer_thread_is_started;
  ten_atomic_t is_stop;
  muxer_t *muxer;

  ten_mutex_t *out_lock;
  ten_event_t *out_available;
  std::list<std::unique_ptr<ten::audio_frame_t>> out_audios;
  std::list<std::unique_ptr<ten::video_frame_t>> out_images;

  demuxer_settings_t settings;
  std::string output_stream;

  bool audio_eof;
  bool video_eof;

  ten::ten_env_proxy_t *ten_env_proxy;
};

}  // namespace ffmpeg_extension
}  // namespace ten
