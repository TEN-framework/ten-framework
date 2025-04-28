//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include <sys/types.h>

#include <cstdint>
#include <string>

#include "ten_runtime/binding/cpp/ten.h"

#if defined(__cplusplus)
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#if defined(__cplusplus)
}
#endif

namespace ten {
namespace ffmpeg_extension {

class demuxer_thread_t;

enum DECODE_STATUS {
  DECODE_STATUS_SUCCESS,
  DECODE_STATUS_EOF,
  DECODE_STATUS_ERROR,
};

struct interrupt_cb_param_t {
  time_t last_time;
};

class demuxer_t {
 public:
  demuxer_t(ten::ten_env_proxy_t *ten_env_proxy,
            demuxer_thread_t *demuxer_thread);
  ~demuxer_t();

  demuxer_t(const demuxer_t &other) = delete;
  demuxer_t(demuxer_t &&other) = delete;

  demuxer_t &operator=(const demuxer_t &other) = delete;
  demuxer_t &operator=(demuxer_t &&other) = delete;

  bool open_input_stream(const std::string &input_stream_loc);

  DECODE_STATUS decode_next_packet();

  int video_width() const;
  int video_height() const;
  int64_t video_bit_rate() const;
  AVRational video_frame_rate() const;
  AVRational video_time_base() const;
  AVRational audio_time_base() const;
  int64_t number_of_video_frames() const;

 private:
  friend class demuxer_thread_t;

  AVFormatContext *create_input_format_context(
      const std::string &input_stream_loc);
  AVFormatContext *create_input_format_context_with_retry(
      const std::string &input_stream_loc);

  void open_video_decoder();
  void open_audio_decoder();

  bool is_av_decoder_opened();

  void dump_video_info();
  void dump_audio_info();

  void flush_remaining_audio_frames();
  void flush_remaining_video_frames();

  bool analyze_input_stream();

  AVCodecParameters *get_video_decoder_params() const;
  AVCodecParameters *get_audio_decoder_params() const;

  bool create_audio_converter(const AVFrame *frame,
                              uint64_t *out_channel_layout,
                              int *out_sample_rate);
  bool create_video_converter(int width, int height);

  std::unique_ptr<ten::audio_frame_t> to_ten_audio_frame(const AVFrame *frame);
  std::unique_ptr<ten::video_frame_t> to_ten_video_frame(const AVFrame *frame);

  bool decode_next_video_packet(DECODE_STATUS &decode_status);
  bool decode_next_audio_packet(DECODE_STATUS &decode_status);

  std::string input_stream_loc;
  demuxer_thread_t *demuxer_thread;
  ten::ten_env_proxy_t *ten_env_proxy;

  // This structure describes the basic information of a media file or media
  // streaming. This is the most basic structure in FFmpeg, which is the root of
  // all other structures. It is the fundamental abstract of a media file or
  // streaming.
  AVFormatContext *input_format_context;

  interrupt_cb_param_t *interrupt_cb_param;

  int video_stream_idx;
  int audio_stream_idx;

  AVCodecContext *video_decoder_ctx;
  AVCodecContext *audio_decoder_ctx;

  const AVCodec *video_decoder;
  const AVCodec *audio_decoder;

  struct SwsContext *video_converter_ctx;
  SwrContext *audio_converter_ctx;

  AVPacket *packet;
  AVFrame *frame;

  // The video format output by the demuxer.
  int rotate_degree;

  // The audio format output by the demuxer.
  int audio_sample_rate;
  uint64_t audio_channel_layout_mask;
  int audio_num_of_channels;
};

}  // namespace ffmpeg_extension
}  // namespace ten
