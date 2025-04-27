//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#pragma once

#include <cstdint>
#include <string>

#if defined(__cplusplus)
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#if defined(__cplusplus)
}
#endif

#include "ten_runtime/binding/cpp/ten.h"

namespace ten {
namespace ffmpeg_extension {

enum ENCODE_STATUS {
  ENCODE_STATUS_SUCCESS,
  ENCODE_STATUS_EOF,
  ENCODE_STATUS_ERROR,
};

class muxer_t {
 public:
  muxer_t();
  ~muxer_t();

  // @{
  muxer_t(const muxer_t &other) = delete;
  muxer_t(muxer_t &&other) = delete;
  muxer_t &operator=(const muxer_t &other) = delete;
  muxer_t &operator=(muxer_t &&other) = delete;
  // @}

  bool open(const std::string &dest_name, bool realtime);

  ENCODE_STATUS encode_video_frame(
      std::unique_ptr<ten::video_frame_t> video_frame);
  ENCODE_STATUS encode_audio_frame(
      std::unique_ptr<ten::audio_frame_t> audio_frame);

  int64_t next_video_timing();

 private:
  friend class muxer_thread_t;

  bool is_av_encoder_opened();

  bool open_video_encoder(bool realtime);
  bool open_audio_encoder();

  void dump_video_info();
  void dump_audio_info();

  int64_t next_video_pts();
  int64_t next_audio_pts();

  bool encode_audio_frame(AVFrame *frame);
  bool encode_audio_silent_frame();

  void flush_remaining_audio_frames();
  void flush_remaining_video_frames();

  bool allocate_audio_fifo(AVCodecParameters *encoded_stream_params);
  bool allocate_audio_frame(AVCodecParameters *encoded_stream_params);

  bool convert_audio_frame(AVCodecParameters *encoded_stream_params,
                           ten::audio_frame_t &ten_audio_frame);
  AVFrame *convert_video_frame(ten::video_frame_t &video_frame);

  bool create_audio_converter(AVCodecParameters *encoded_stream_params,
                              ten::audio_frame_t &ten_audio_frame);
  bool create_video_converter(ten::video_frame_t &video_frame);

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

  std::string dest_name;

  AVFormatContext *output_format_ctx;

  unsigned int video_stream_idx;
  unsigned int audio_stream_idx;

  int64_t next_video_idx;  // Next video frame index in video encoder context.
  int64_t next_audio_idx;  // Next audio sample index in audio encoder context.

  AVCodecContext *video_encoder_ctx;
  AVCodecContext *audio_encoder_ctx;

  const AVCodec *video_encoder;
  const AVCodec *audio_encoder;

  AVStream *video_stream;
  AVStream *audio_stream;

  struct SwsContext *video_converter_ctx;
  struct SwrContext *audio_converter_ctx;

  AVPacket *packet;
  AVAudioFifo *audio_fifo;
  AVFrame *audio_frame;
  uint64_t audio_prepend_pts;
};

}  // namespace ffmpeg_extension
}  // namespace ten
