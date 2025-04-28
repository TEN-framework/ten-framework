//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "muxer.h"

#include <libavcodec/packet.h>
#include <libavutil/channel_layout.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lang/cpp/lib/buf.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

// Convert between milliseconds and PTS (Presentation Timestamp) based on the
// stream's time base.
#define ms2pts(pts, stream)                \
  av_rescale(pts, (stream)->time_base.den, \
             static_cast<int64_t>((stream)->time_base.num) * 1000)

#define pts2ms(ms, stream)                                             \
  av_rescale(ms, static_cast<int64_t>((stream)->time_base.num) * 1000, \
             (stream)->time_base.den)

#define GET_FFMPEG_ERROR_MESSAGE(err_msg, errnum)                           \
  /* NOLINTNEXTLINE */                                                      \
  for (char err_msg[AV_ERROR_MAX_STRING_SIZE], times = 0;                   \
       get_ffmpeg_error_message(err_msg, AV_ERROR_MAX_STRING_SIZE, errnum), \
                                               times == 0;                  \
       ++times)

#define DEMUXER_OUTPUT_VIDEO_FRAME_PIXEL_FMT AV_PIX_FMT_RGB24

// @{
// Output video settings.
#define OUTPUT_VIDEO_CODEC AV_CODEC_ID_H264
#define OUTPUT_VIDEO_PIXEL_FMT AV_PIX_FMT_YUV420P
// 1 I-frame for every 10 frames at most
#define OUTPUT_VIDEO_GOP_SIZE 10
// Output will be delayed by OUTPUT_VIDEO_MAX_B_FRAMES + 1
#define OUTPUT_VIDEO_MAX_B_FRAMES 10
// @}

// @{
// Output audio settings.
#define OUTPUT_AUDIO_CODEC AV_CODEC_ID_AAC
#define OUTPUT_AUDIO_FORMAT AV_SAMPLE_FMT_FLTP
#define OUTPUT_AUDIO_SAMPLE_RATE 48000
#define OUTPUT_AUDIO_CHANNEL_MASK AV_CH_LAYOUT_STEREO
// @}

namespace ten {
namespace ffmpeg_extension {

namespace {

void get_ffmpeg_error_message(char *buf, size_t buf_size, int errnum) {
  TEN_ASSERT(buf && buf_size, "Invalid argument.");

  // Get error from ffmpeg.
  if (av_strerror(errnum, buf, buf_size) != 0) {
    int written =
        snprintf(buf, buf_size, "Unknown ffmpeg error code: %d", errnum);
    TEN_ASSERT(written > 0, "Should not happen.");
  }
}

AVFrame *yuv_frame_create(int width, int height) {
  AVFrame *av_frame = av_frame_alloc();
  TEN_ASSERT(av_frame, "Failed to create AVframe.");

  av_frame->width = width;
  av_frame->height = height;
  av_frame->format = OUTPUT_VIDEO_PIXEL_FMT;

  av_image_alloc(av_frame->data, av_frame->linesize, width, height,
                 OUTPUT_VIDEO_PIXEL_FMT, 32);

  return av_frame;
}

void yuv_frame_destroy(AVFrame *av_frame) {
  if (av_frame != nullptr) {
    av_freep(&(av_frame->data[0]));
    av_frame_free(&av_frame);
  }
}

// Debug purpose only.
TEN_UNUSED void save_avframe(const AVFrame *avFrame) {
  FILE *fDump = fopen("encode", "ab");

  uint32_t pitchY = avFrame->linesize[0];
  uint32_t pitchU = avFrame->linesize[1];
  uint32_t pitchV = avFrame->linesize[2];

  uint8_t *avY = avFrame->data[0];
  uint8_t *avU = avFrame->data[1];
  uint8_t *avV = avFrame->data[2];

  for (int i = 0; i < avFrame->height; i++) {
    (void)fwrite(avY, avFrame->width, 1, fDump);
    avY += pitchY;
  }

  for (int i = 0; i < avFrame->height / 2; i++) {
    (void)fwrite(avU, avFrame->width / 2, 1, fDump);
    avU += pitchU;
  }

  for (int i = 0; i < avFrame->height / 2; i++) {
    (void)fwrite(avV, avFrame->width / 2, 1, fDump);
    avV += pitchV;
  }

  (void)fclose(fDump);
}

// Debug purpose only.
TEN_UNUSED void save_video_frame(
    const std::shared_ptr<ten::video_frame_t> &pFrame, int index) {
  FILE *pFile = nullptr;
  char szFilename[32];
  int y = 0;

  int width = pFrame->get_width();
  int height = pFrame->get_height();

  // Open file
  int written = snprintf(szFilename, 32, "encode_frame%d.ppm", index);
  TEN_ASSERT(written > 0, "Should not happen.");

  pFile = fopen(szFilename, "wb");

  if (pFile == nullptr) {
    return;
  }

  // Write header
  (void)fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  ten::buf_t locked_buf = pFrame->lock_buf();
  for (y = 0; y < height; y++) {
    (void)fwrite(locked_buf.data() + static_cast<ptrdiff_t>(y * width * 3), 1,
                 static_cast<int64_t>(width * 3), pFile);
  }
  pFrame->unlock_buf(locked_buf);

  // Close file
  (void)fclose(pFile);
}

}  // namespace

muxer_t::muxer_t()
    : src_video_width(0),
      src_video_height(0),
      src_video_bit_rate(0),
      src_video_number_of_frames(0),
      src_video_frame_rate({}),
      src_video_time_base({}),
      src_audio_sample_rate(0),
      src_audio_time_base({}),
      src_audio_channel_layout_mask(0),
      output_format_ctx(nullptr),
      video_stream_idx(-1),
      audio_stream_idx(-1),
      next_video_idx(0),
      next_audio_idx(0),
      video_encoder_ctx(nullptr),
      audio_encoder_ctx(nullptr),
      video_encoder(nullptr),
      audio_encoder(nullptr),
      video_stream(nullptr),
      audio_stream(nullptr),
      video_converter_ctx(nullptr),
      audio_converter_ctx(nullptr),
      packet(av_packet_alloc()),
      audio_fifo(nullptr),
      audio_frame(nullptr),
      audio_prepend_pts(0) {}

muxer_t::~muxer_t() {
  if (audio_frame != nullptr) {
    av_frame_free(&audio_frame);
  }
  if (packet != nullptr) {
    av_packet_free(&packet);
  }

  if (output_format_ctx != nullptr && output_format_ctx->pb != nullptr) {
    // Write trailer information.
    av_write_trailer(output_format_ctx);
  }

  if (video_encoder_ctx != nullptr) {
    avcodec_free_context(&video_encoder_ctx);
    avcodec_free_context(&video_encoder_ctx);
  }

  if (audio_encoder_ctx != nullptr) {
    avcodec_free_context(&audio_encoder_ctx);
    avcodec_free_context(&audio_encoder_ctx);
  }

  if (video_converter_ctx != nullptr) {
    sws_freeContext(video_converter_ctx);
  }
  if (audio_converter_ctx != nullptr) {
    swr_free(&audio_converter_ctx);
  }
  if (audio_fifo != nullptr) {
    av_audio_fifo_free(audio_fifo);
  }

  if (output_format_ctx != nullptr) {
    if (output_format_ctx->pb != nullptr) {
      // Close the output file.
      avio_close(output_format_ctx->pb);
    }

    // Free output context.
    avformat_free_context(output_format_ctx);
  }
}

bool muxer_t::is_av_encoder_opened() {
  return video_encoder != nullptr || audio_encoder != nullptr;
}

// Convert the video index in the video encoder context to the video index in
// the video output stream.
//
// (next_video_idx / framerate) = x seconds totally.
// x seconds / time_base = pts according to the time_base of the output
// stream.
int64_t muxer_t::next_video_pts() {
  return av_rescale_q(next_video_idx, av_inv_q(video_encoder_ctx->framerate),
                      video_stream->time_base);
}

// Convert the audio index in the audio encoder context to the audio index in
// the audio output stream.
//
// (next_audio_idx / sample_rate) = x seconds totally
// x seconds / time_base
// = x seconds * (1/time_base)
// = x seconds * time_base.den / time_base.num
// = pts according to the time_base of the output stream
int64_t muxer_t::next_audio_pts() {
  return av_rescale(next_audio_idx, audio_stream->time_base.den,
                    static_cast<int64_t>(audio_stream->time_base.num) *
                        audio_encoder_ctx->sample_rate);
}

void muxer_t::flush_remaining_audio_frames() {
  // Send a `nullptr` frame to the audio encoder to indicate the end of
  // encoding.
  int ffmpeg_rc = avcodec_send_frame(audio_encoder_ctx, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to flush audio frame: %s", err_msg);
    }
    return;
  }

  // Retrieve all remaining encoded packets.
  while (ffmpeg_rc >= 0) {
    // Discard the previous received audio packet.
    av_packet_unref(packet);

    ffmpeg_rc = avcodec_receive_packet(audio_encoder_ctx, packet);
    if (ffmpeg_rc < 0) {
      if (ffmpeg_rc == AVERROR(EAGAIN)) {
        ffmpeg_rc = 0;
        continue;
      } else {
        GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
          TEN_LOGE("Failed to flush audio frame: %s", err_msg);
        }
        return;
      }
    }

    packet->stream_index = static_cast<int>(audio_stream_idx);

    TEN_LOGD("Encoded an audio packet after flushing, pts=%" PRId64
             ", dts=%" PRId64 ", size=%d",
             packet->pts, packet->dts, packet->size);

    // Write a packet to a output media file ensuring correct interleaving.
    ffmpeg_rc = av_interleaved_write_frame(output_format_ctx, packet);
    if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGD("Error writing audio packet: %s", err_msg);
      }
    }
  }
}

bool muxer_t::encode_audio_frame(AVFrame *av_frame) {
  TEN_ASSERT(audio_encoder_ctx && output_format_ctx, "Invalid argument.");

  // Supply a raw audio frame to the audio encoder.
  int ffmpeg_rc = avcodec_send_frame(audio_encoder_ctx, av_frame);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to encode a audio frame: %s", err_msg);
    }
    if (ffmpeg_rc == AVERROR_EOF) {
      TEN_LOGD("encode an EOF audio packet.");
      return true;
    }
    return false;
  }

  // Discard the previous received audio packet.
  av_packet_unref(packet);

  // Read encoded data from the encoder.
  ffmpeg_rc = avcodec_receive_packet(audio_encoder_ctx, packet);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to encode an audio packet: %s", err_msg);
    }
    if (ffmpeg_rc == AVERROR(EAGAIN)) {
      TEN_LOGE(
          "Failed to encode an audio packet: \
      need more frame to produce output packet (audio case)");
      return true;
    }
    if (ffmpeg_rc == AVERROR_EOF) {
      TEN_LOGD("encode an EOF audio packet.");
      return true;
    }
    return false;
  }

  packet->stream_index = static_cast<int>(audio_stream_idx);

  // Write a packet to a output media file ensuring correct interleaving.
  ffmpeg_rc = av_interleaved_write_frame(output_format_ctx, packet);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to write a audio packet to the output stream: %s",
               err_msg);
    }
    return false;
  }

  return true;
}

bool muxer_t::encode_audio_silent_frame() {
  if (audio_encoder == nullptr) {
    TEN_LOGE("Must open audio stream first");
    return false;
  }

  // The encoding parameters of the audio stream.
  AVCodecParameters *encoded_stream_params = audio_stream->codecpar;

  if (audio_frame == nullptr) {
    audio_frame = av_frame_alloc();
    if (audio_frame == nullptr) {
      TEN_LOGE("Failed to allocate audio frame.");
      return false;
    }

    audio_frame->nb_samples = encoded_stream_params->frame_size;
    audio_frame->format = encoded_stream_params->format;

    av_channel_layout_copy(&audio_frame->ch_layout,
                           &encoded_stream_params->ch_layout);

    if (av_frame_get_buffer(audio_frame, 32) < 0) {
      TEN_LOGE("Failed to allocate audio frame");
      return false;
    }
  }

  av_frame_make_writable(audio_frame);
  av_samples_set_silence(audio_frame->data, 0, audio_frame->nb_samples,
                         audio_frame->ch_layout.nb_channels,
                         static_cast<enum AVSampleFormat>(audio_frame->format));

  audio_frame->pts = next_audio_pts();
  next_audio_idx += audio_frame->nb_samples;

  TEN_LOGD("Encode a silent audio frame, pts=%" PRId64, audio_frame->pts);

  return encode_audio_frame(audio_frame);
}

// The parameters of the stream we push back should be equal to the parameters
// of the original stream we pull.
//
// dest_name: The path of the output file.
bool muxer_t::open(const std::string &dest_name, const bool realtime) {
  if (is_av_encoder_opened()) {
    TEN_LOGD("Muxer already opened");
    return true;
  }

  TEN_LOGD("Preparing to open output stream [%s]", dest_name.c_str());

  // Create encoders.
  //
  // FLV is used for real-time, while MP4 is used for non-real-time.
  const char *format_str = realtime ? "flv" : "mp4";

  this->dest_name = dest_name;

  int ffmpeg_rc = avformat_alloc_output_context2(&output_format_ctx, nullptr,
                                                 format_str, dest_name.c_str());
  if (ffmpeg_rc < 0 || (output_format_ctx == nullptr)) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to open output stream: cannot alloc output context: %s",
               err_msg);
    }
    return false;
  }

  open_video_encoder(realtime);
  open_audio_encoder();

  if (!is_av_encoder_opened()) {
    TEN_LOGD("Failed to open encoders");
    return false;
  }

  // Open output stream.
  // NOLINTNEXTLINE(hicpp-signed-bitwise)
  if ((output_format_ctx->oformat->flags & AVFMT_NOFILE) == 0) {
    ffmpeg_rc =
        avio_open2(&output_format_ctx->pb, dest_name.c_str(), AVIO_FLAG_WRITE,
                   &output_format_ctx->interrupt_callback, nullptr);
    if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGD("Failed to open output: %s", err_msg);
      }
      return false;
    }
  }

  // Write the header information of the output file.
  ffmpeg_rc = avformat_write_header(output_format_ctx, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to write output header: %s", err_msg);
    }
    return false;
  }

  TEN_LOGD("Output stream [%s] is opened", dest_name.c_str());

  // Encode 2 silent frames for the AAC encoder.
  encode_audio_silent_frame();
  encode_audio_silent_frame();

  audio_prepend_pts = next_audio_pts();

  return true;
}

void muxer_t::dump_video_info() {
  TEN_LOGD("v:width:       %d", video_encoder_ctx->width);
  TEN_LOGD("v:height:      %d", video_encoder_ctx->height);
  TEN_LOGD("v:bit_rate:    %" PRId64, video_encoder_ctx->bit_rate);
  TEN_LOGD("v:rc_min_rate: %" PRId64, video_encoder_ctx->rc_min_rate);
  TEN_LOGD("v:rc_max_rate: %" PRId64, video_encoder_ctx->rc_max_rate);
  TEN_LOGD("v:time_base:   %d/%d", video_encoder_ctx->time_base.num,
           video_encoder_ctx->time_base.den);
  TEN_LOGD("v:pix_fmt:     %d", video_encoder_ctx->pix_fmt);
  TEN_LOGD("v:framerate:   %d/%d", video_encoder_ctx->framerate.num,
           video_encoder_ctx->framerate.den);
  TEN_LOGD("v:time_base:   %d/%d", video_stream->time_base.num,
           video_stream->time_base.den);
}

void muxer_t::dump_audio_info() {
  TEN_LOGD("a:sample_fmt:     %d", audio_encoder_ctx->sample_fmt);
  TEN_LOGD("a:sample_rate:    %d", audio_encoder_ctx->sample_rate);
  TEN_LOGD("a:channels:       %d", audio_encoder_ctx->ch_layout.nb_channels);
  TEN_LOGD("a:time_base:      %d/%d", audio_encoder_ctx->time_base.num,
           audio_encoder_ctx->time_base.den);
  TEN_LOGD("a:bit_rate:       %" PRId64, audio_encoder_ctx->bit_rate);
  TEN_LOGD("a:time_base:      %d/%d", audio_stream->time_base.num,
           audio_stream->time_base.den);
  TEN_LOGD("a:frame_size:     %d", audio_stream->codecpar->frame_size);
}

bool muxer_t::open_video_encoder(const bool realtime) {
  TEN_ASSERT(output_format_ctx, "Invalid argument.");

  // The ownership of 'video_encoder' belongs to ffmpeg, and it's not
  // necessary to delete it when encountering errors.
  video_encoder = avcodec_find_encoder(OUTPUT_VIDEO_CODEC);
  if (video_encoder == nullptr) {
    TEN_LOGE("Video encoder not supported: %s",
             avcodec_get_name(OUTPUT_VIDEO_CODEC));
    return false;
  }

  // Add video stream to the output.
  video_stream = avformat_new_stream(output_format_ctx, video_encoder);
  if (video_stream == nullptr) {
    TEN_LOGE("Failed to open video output stream: %s", dest_name.c_str());
    return false;
  }

  video_stream_idx = output_format_ctx->nb_streams - 1;
  video_stream->id = static_cast<int>(video_stream_idx);

  // Initialize video codec context.
  video_encoder_ctx = avcodec_alloc_context3(video_encoder);
  if (video_encoder_ctx == nullptr) {
    TEN_LOGE("Failed to allocate video encoder context");
    return false;
  }

  AVRational framerate = src_video_frame_rate;
  AVRational time_base;
  if (realtime) {
    time_base = av_inv_q(framerate);
  } else {
    time_base = src_video_time_base;
  }

  video_encoder_ctx->codec_id = OUTPUT_VIDEO_CODEC;
  video_encoder_ctx->width = src_video_width;
  video_encoder_ctx->height = src_video_height;
  video_encoder_ctx->bit_rate = src_video_bit_rate;
  video_encoder_ctx->time_base = time_base;
  video_encoder_ctx->gop_size = OUTPUT_VIDEO_GOP_SIZE;
  video_encoder_ctx->max_b_frames = OUTPUT_VIDEO_MAX_B_FRAMES;
  video_encoder_ctx->pix_fmt = OUTPUT_VIDEO_PIXEL_FMT;
  video_encoder_ctx->framerate = framerate;
  video_encoder_ctx->sample_aspect_ratio = (AVRational){0, 1};
  video_encoder_ctx->profile = FF_PROFILE_H264_HIGH;
  video_encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  video_stream->time_base = video_encoder_ctx->time_base;
  video_stream->avg_frame_rate = video_encoder_ctx->framerate;
  video_stream->sample_aspect_ratio = video_encoder_ctx->sample_aspect_ratio;

  // Enable video codec.
  AVDictionary *av_options = nullptr;
  if (realtime) {
    // If it is real-time encoding, configure the encoder with tuning options
    // such as `"zerolatency"`.
    av_dict_set(&av_options, "tune", "zerolatency", 0);

    // The following 'preset' would affect the encoding quality.
    av_dict_set(&av_options, "preset", "veryfast", 0);
    av_dict_set(&av_options, "profile", "high", 0);
    av_dict_set(&av_options, "level", "5.0", 0);

    // The following is the suggestions from youtube:
    // 854*480*30   = 12000k, youtube:  500k (1/24) - 2000k (1/6)
    // 1280*720*30  = 27000k, youtube: 1500k (1/18) - 4000k (1/6.75)
    // 1280*720*60  = 54000k, youtube: 2250k (1/24) - 4000k (1/9)
    // 1920*1080*30 = 60750k, youtube: 3000k (1/20) - 6000k (1/10)
    //
    // So we set the 'compression ratio' to 1/20 ~ 1/15 ~ 1/12.
    int64_t min_compression_ratio = 20;
    int64_t prefer_compression_ratio = 15;
    int64_t max_compression_ratio = 12;

    int64_t min_rate =
        av_rescale(static_cast<int64_t>(video_encoder_ctx->width) *
                       video_encoder_ctx->height,
                   framerate.num, framerate.den) /
        min_compression_ratio;
    int64_t prefer_rate =
        av_rescale(static_cast<int64_t>(video_encoder_ctx->width) *
                       video_encoder_ctx->height,
                   framerate.num, framerate.den) /
        prefer_compression_ratio;
    int64_t max_rate =
        av_rescale(static_cast<int64_t>(video_encoder_ctx->width) *
                       video_encoder_ctx->height,
                   framerate.num, framerate.den) /
        max_compression_ratio;
    video_encoder_ctx->rc_min_rate = min_rate;
    video_encoder_ctx->rc_max_rate = max_rate;
    video_encoder_ctx->rc_buffer_size = static_cast<int>(max_rate * 2);

    if (video_encoder_ctx->bit_rate < prefer_rate) {
      TEN_LOGD("Raise bitrate from %" PRId64 " to %" PRId64,
               video_encoder_ctx->bit_rate, prefer_rate);
      video_encoder_ctx->bit_rate = prefer_rate;
    }
  } else {
    video_encoder_ctx->gop_size = 250;    // Follow ffmpeg default setting
    video_encoder_ctx->max_b_frames = 3;  // Follow ffmpeg default setting
    video_stream->start_time = 0;

    int64_t frame_dur = av_rescale_q(1, av_inv_q(framerate), time_base);
    video_stream->duration = src_video_number_of_frames * frame_dur;
  }

  // Open the video encoder.
  int ffmpeg_rc = avcodec_open2(video_encoder_ctx, video_encoder, &av_options);
  av_dict_free(&av_options);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to open video codec: %s", err_msg);
    }
    return false;
  }

  // Copy the parameters from the encoder context to the video stream's
  // encoding parameters.
  ffmpeg_rc = avcodec_parameters_from_context(video_stream->codecpar,
                                              video_encoder_ctx);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to copy video codec parameters: %s", err_msg);
    }
    return false;
  }

  dump_video_info();

  TEN_LOGD("%s video encoder opened for stream %d", video_encoder->name,
           video_stream_idx);

  return true;
}

bool muxer_t::open_audio_encoder() {
  audio_encoder = avcodec_find_encoder(OUTPUT_AUDIO_CODEC);
  if (audio_encoder == nullptr) {
    TEN_LOGE("Audio encoder not supported: %s",
             avcodec_get_name(OUTPUT_AUDIO_CODEC));
    return false;
  }

  // Add audio stream to the output.
  audio_stream = avformat_new_stream(output_format_ctx, audio_encoder);
  if (audio_stream == nullptr) {
    TEN_LOGE("Failed to open audio output stream: %s", dest_name.c_str());
    return false;
  }

  audio_stream_idx = output_format_ctx->nb_streams - 1;
  audio_stream->id = static_cast<int>(audio_stream_idx);

  // Initialize audio codec context.
  audio_encoder_ctx = avcodec_alloc_context3(audio_encoder);
  if (audio_encoder_ctx == nullptr) {
    TEN_LOGE("Failed to allocate audio encoder context");
    return false;
  }

  AVSampleFormat sample_fmt = OUTPUT_AUDIO_FORMAT;  // fallback
  {
    const void *configs = nullptr;
    int num_configs = 0;

    // AV_CODEC_CONFIG_SAMPLE_FORMAT indicates that obtaining supportable sample
    // fmt.
    int ret = avcodec_get_supported_config(nullptr, audio_encoder,
                                           AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                           &configs, &num_configs);

    if (ret >= 0 && num_configs > 0 && configs != nullptr) {
      const auto *avail_fmts =
          reinterpret_cast<const AVSampleFormat *>(configs);

      // At present, the first one is used directly, and we can also loop them
      // and choose a more suitable one in the future.
      sample_fmt = avail_fmts[0];
    } else {
      TEN_LOGW(
          "No supported sample_fmt found by avcodec_get_supported_config, "
          "fallback to OUTPUT_AUDIO_FORMAT");
    }
  }

  int sample_rate = OUTPUT_AUDIO_SAMPLE_RATE;
  if (src_audio_sample_rate > 0) {
    sample_rate = src_audio_sample_rate;
  }

  AVRational time_base = (AVRational){1, audio_encoder_ctx->sample_rate};
  if (src_audio_time_base.num > 0) {
    time_base = src_audio_time_base;
  }

  audio_encoder_ctx->sample_fmt = sample_fmt;
  audio_encoder_ctx->sample_rate = sample_rate;

  AVChannelLayout desired_ch_layout;
  if (src_audio_channel_layout_mask != 0) {
    av_channel_layout_from_mask(&desired_ch_layout,
                                src_audio_channel_layout_mask);
  } else {
    av_channel_layout_from_mask(&desired_ch_layout, OUTPUT_AUDIO_CHANNEL_MASK);
  }
  av_channel_layout_copy(&audio_encoder_ctx->ch_layout, &desired_ch_layout);

  audio_encoder_ctx->time_base = time_base;

  audio_stream->time_base = time_base;

  // Enable audio codec.
  AVDictionary *av_options = nullptr;
  int ffmpeg_rc = avcodec_open2(audio_encoder_ctx, audio_encoder, &av_options);
  av_dict_free(&av_options);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to open audio codec: %s", err_msg);
    }
    return false;
  }

  ffmpeg_rc = avcodec_parameters_from_context(audio_stream->codecpar,
                                              audio_encoder_ctx);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to copy audio codec parameters: %s", err_msg);
    }
    return false;
  }

  dump_audio_info();

  TEN_LOGD("%s audio encoder opened for stream %d", audio_encoder->name,
           audio_stream_idx);

  return true;
}

bool muxer_t::create_video_converter(ten::video_frame_t &video_frame) {
  if (video_converter_ctx == nullptr) {
    int width = video_frame.get_width();
    int height = video_frame.get_height();

    video_converter_ctx = sws_getContext(
        width, height, DEMUXER_OUTPUT_VIDEO_FRAME_PIXEL_FMT, width, height,
        OUTPUT_VIDEO_PIXEL_FMT, SWS_POINT, nullptr, nullptr, nullptr);
    if (video_converter_ctx == nullptr) {
      TEN_LOGD(
          "Failed to create converter context to convert from RGB frame \
      to YUV frame.");
      return false;
    }
  }

  return true;
}

AVFrame *muxer_t::convert_video_frame(ten::video_frame_t &video_frame) {
  // This is an EOF frame.
  if (video_frame.is_eof()) {
    return nullptr;
  }

  if (video_frame.get_pixel_fmt() == TEN_PIXEL_FMT_I420) {
    auto frame_width = video_frame.get_width();
    auto frame_height = video_frame.get_height();

    ten::buf_t locked_buf = video_frame.lock_buf();

    auto *y_data = locked_buf.data();
    auto *u_data = y_data + static_cast<ptrdiff_t>(frame_width * frame_height);
    auto *v_data = u_data + (frame_width * frame_height / 4);

    AVFrame *yuv_frame = yuv_frame_create(frame_width, frame_height);
    TEN_ASSERT(yuv_frame, "Failed to create YUV frame.");

    av_image_get_buffer_size(OUTPUT_VIDEO_PIXEL_FMT, frame_width, frame_height,
                             16);
    av_image_copy_plane(yuv_frame->data[0], yuv_frame->linesize[0], y_data,
                        frame_width, frame_width, frame_height);
    av_image_copy_plane(yuv_frame->data[1], yuv_frame->linesize[1], u_data,
                        frame_width / 2, frame_width / 2, frame_height / 2);
    av_image_copy_plane(yuv_frame->data[2], yuv_frame->linesize[2], v_data,
                        frame_width / 2, frame_width / 2, frame_height / 2);

    video_frame.unlock_buf(locked_buf);

    return yuv_frame;
  }

  if (video_frame.get_pixel_fmt() == TEN_PIXEL_FMT_RGB24) {
    if (!create_video_converter(video_frame)) {
      return nullptr;
    }

    ten::buf_t locked_buf = video_frame.lock_buf();

    const uint8_t *rgb_data[1] = {locked_buf.data()};     // NOLINT
    int rgb_linesize[1] = {video_frame.get_width() * 3};  // NOLINT

    AVFrame *yuv_frame =
        yuv_frame_create(video_frame.get_width(), video_frame.get_height());
    TEN_ASSERT(yuv_frame, "Failed to create YUV frame.");

    sws_scale(video_converter_ctx, rgb_data, rgb_linesize, 0,
              video_frame.get_height(), yuv_frame->data, yuv_frame->linesize);

    video_frame.unlock_buf(locked_buf);

    return yuv_frame;
  }

  return nullptr;
}

void muxer_t::flush_remaining_video_frames() {
  int ffmpeg_rc = avcodec_send_frame(video_encoder_ctx, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to flush video frame: %s", err_msg);
    }
    return;
  }

  // Retrieve all remaining encoded packets.
  while (ffmpeg_rc >= 0) {
    // Discard the previous received video packet.
    av_packet_unref(packet);

    ffmpeg_rc = avcodec_receive_packet(video_encoder_ctx, packet);
    if (ffmpeg_rc < 0) {
      if (ffmpeg_rc == AVERROR(EAGAIN)) {
        ffmpeg_rc = 0;
        continue;
      } else {
        GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
          TEN_LOGE("Failed to flush video frame: %s", err_msg);
        }
        return;
      }
    }

    packet->stream_index = static_cast<int>(video_stream_idx);
    packet->duration =
        av_rescale_q(packet->duration, video_encoder_ctx->time_base,
                     video_stream->time_base);
    packet->pts = av_rescale_q(packet->pts, video_encoder_ctx->time_base,
                               video_stream->time_base);
    packet->dts = av_rescale_q(packet->dts, video_encoder_ctx->time_base,
                               video_stream->time_base);

    // Write the packet to the output.
    ffmpeg_rc = av_interleaved_write_frame(output_format_ctx, packet);
    if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGD("Error writing video packet: %s", err_msg);
      }
    }
  }
}

ENCODE_STATUS muxer_t::encode_video_frame(
    std::unique_ptr<ten::video_frame_t> video_frame) {
  if (video_encoder == nullptr) {
    TEN_LOGE("Must open video stream first");
    return ENCODE_STATUS_ERROR;
  }

  AVFrame *yuv_frame = convert_video_frame(*video_frame);
  if (yuv_frame != nullptr) {
    yuv_frame->pts = next_video_pts();
    next_video_idx++;

    // DEBUG
    // save_avframe(yuv_frame);
    // save_img_frame(video_frame, yuv_frame->pts);
  } else {
    // EOF frame.
    flush_remaining_video_frames();
    return ENCODE_STATUS_EOF;
  }

  int ffmpeg_rc = avcodec_send_frame(video_encoder_ctx, yuv_frame);
  if (ffmpeg_rc < 0) {
    yuv_frame_destroy(yuv_frame);
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to encode a video frame: %s", err_msg);
    }

    if (ffmpeg_rc == AVERROR_EOF) {
      TEN_LOGD("encode an EOF video packet.");
      return ENCODE_STATUS_ERROR;
    }
    return ENCODE_STATUS_ERROR;
  }

  // Discard the previous received video packet.
  av_packet_unref(packet);

  // Retrieve the encoded packet.
  ffmpeg_rc = avcodec_receive_packet(video_encoder_ctx, packet);
  if (ffmpeg_rc < 0) {
    yuv_frame_destroy(yuv_frame);

    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to encode a video packet: %s", err_msg);
    }

    if (ffmpeg_rc == AVERROR(EAGAIN)) {
      TEN_LOGE(
          "Failed to encode a video packet: \
      need more frame to produce output packet (b-frame's case)");
      return ENCODE_STATUS_SUCCESS;
    }

    if (ffmpeg_rc == AVERROR_EOF) {
      TEN_LOGD("encode an EOF video packet.");
      return ENCODE_STATUS_ERROR;
    }

    return ENCODE_STATUS_ERROR;
  }

  packet->stream_index = static_cast<int>(video_stream_idx);
  packet->duration = av_rescale_q(
      packet->duration, video_encoder_ctx->time_base, video_stream->time_base);
  packet->pts = av_rescale_q(packet->pts, video_encoder_ctx->time_base,
                             video_stream->time_base);
  packet->dts = av_rescale_q(packet->dts, video_encoder_ctx->time_base,
                             video_stream->time_base);

  TEN_LOGD("Encoded a video packet, pts=%" PRId64 ", dts=%" PRId64 ", size=%d",
           packet->pts, packet->dts, packet->size);

  // Write the encoded video packet to the output stream.
  ffmpeg_rc = av_interleaved_write_frame(output_format_ctx, packet);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Error writing video packet: %s", err_msg);
    }

    yuv_frame_destroy(yuv_frame);

    return ENCODE_STATUS_ERROR;
  }

  yuv_frame_destroy(yuv_frame);
  return ENCODE_STATUS_SUCCESS;
}

// Because the frame_size between the original audio and the requirement of
// the target audio codec would be different, we need a FIFO to queue samples.
bool muxer_t::allocate_audio_fifo(AVCodecParameters *encoded_stream_params) {
  if (audio_fifo == nullptr) {
    audio_fifo = av_audio_fifo_alloc(
        static_cast<enum AVSampleFormat>(encoded_stream_params->format),
        encoded_stream_params->ch_layout.nb_channels,
        encoded_stream_params->frame_size);
    if (audio_fifo == nullptr) {
      TEN_LOGW("Failed to create audio FIFO.");
      return false;
    }
  }
  return true;
}

bool muxer_t::allocate_audio_frame(AVCodecParameters *encoded_stream_params) {
  if (audio_frame == nullptr) {
    audio_frame = av_frame_alloc();
    if (audio_frame == nullptr) {
      TEN_LOGW("Failed to allocate audio frame.");
      return false;
    }

    audio_frame->nb_samples = encoded_stream_params->frame_size;
    audio_frame->format = encoded_stream_params->format;
    av_channel_layout_copy(&audio_frame->ch_layout,
                           &encoded_stream_params->ch_layout);
    if (av_frame_get_buffer(audio_frame, 32) < 0) {
      TEN_LOGW("Failed to allocate audio frame.");
      return false;
    }
  }
  return true;
}

// Initialize audio resampler.
bool muxer_t::create_audio_converter(AVCodecParameters *encoded_stream_params,
                                     ten::audio_frame_t &ten_audio_frame) {
  if (audio_converter_ctx == nullptr) {
    audio_converter_ctx = swr_alloc();
    if (audio_converter_ctx == nullptr) {
      TEN_LOGW("Failed to create audio resampler.");
      return false;
    }

    auto sample_format = AV_SAMPLE_FMT_S16;
    auto bytes_per_sample = ten_audio_frame.get_bytes_per_sample();
    auto data_fmt = ten_audio_frame.get_data_fmt();

    if (data_fmt == TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE) {
      if (bytes_per_sample == 2) {
        sample_format = AV_SAMPLE_FMT_S16;
      } else {
        TEN_ASSERT(0, "not support yet.");
      }
    } else {
      if (bytes_per_sample == 4) {
        sample_format = AV_SAMPLE_FMT_FLTP;
      } else {
        TEN_ASSERT(0, "not support yet.");
      }
    }

    AVChannelLayout in_ch_layout;
    av_channel_layout_from_mask(&in_ch_layout,
                                ten_audio_frame.get_channel_layout());
    av_opt_set_chlayout(audio_converter_ctx, "in_chlayout", &in_ch_layout, 0);

    AVChannelLayout out_ch_layout;
    av_channel_layout_copy(&out_ch_layout, &encoded_stream_params->ch_layout);
    av_opt_set_chlayout(audio_converter_ctx, "out_chlayout", &out_ch_layout, 0);

    av_opt_set_int(audio_converter_ctx, "in_sample_rate",
                   ten_audio_frame.get_sample_rate(), 0);
    av_opt_set_int(audio_converter_ctx, "out_sample_rate",
                   encoded_stream_params->sample_rate, 0);

    av_opt_set_sample_fmt(audio_converter_ctx, "in_sample_fmt", sample_format,
                          0);
    av_opt_set_sample_fmt(
        audio_converter_ctx, "out_sample_fmt",
        static_cast<enum AVSampleFormat>(encoded_stream_params->format), 0);

    TEN_LOGD("Audio resampler setting: from [%d, %d] to [%d, %d]",
             audio_stream->codecpar->sample_rate, sample_format,
             audio_stream->codecpar->sample_rate,
             audio_stream->codecpar->format);

    int ffmpeg_rc = swr_init(audio_converter_ctx);
    if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGD("Failed to initialize resampler: %s", err_msg);
      }
      return false;
    }
  }
  return true;
}

bool muxer_t::convert_audio_frame(AVCodecParameters *encoded_stream_params,
                                  ten::audio_frame_t &ten_audio_frame) {
  uint8_t **dst_channels = nullptr;
  int32_t dst_nb_samples = ten_audio_frame.get_samples_per_channel();

  // Some of the following functions would require a 'dst_nb_samples' of 'int'
  // type, so we check the value of it so that the casting would be safe.
  TEN_ASSERT(dst_nb_samples <= INT_MAX, "Should not happen.");

  int ffmpeg_rc = av_samples_alloc_array_and_samples(
      &dst_channels, nullptr, encoded_stream_params->ch_layout.nb_channels,
      static_cast<int>(dst_nb_samples),
      static_cast<enum AVSampleFormat>(encoded_stream_params->format), 0);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to allocate audio sample: %s", err_msg);
    }
    return false;
  }

  uint8_t *in[8] = {nullptr};
  ten::buf_t locked_in_buf = ten_audio_frame.lock_buf();
  in[0] = locked_in_buf.data();

  ffmpeg_rc = swr_convert(audio_converter_ctx, dst_channels, dst_nb_samples,
                          const_cast<const uint8_t **>(in),
                          ten_audio_frame.get_samples_per_channel());
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to convert audio samples: %s", err_msg);
    }

    ten_audio_frame.unlock_buf(locked_in_buf);

    return false;
  }

  ten_audio_frame.unlock_buf(locked_in_buf);

  ffmpeg_rc =
      av_audio_fifo_realloc(audio_fifo, av_audio_fifo_size(audio_fifo) +
                                            static_cast<int>(dst_nb_samples));
  if (ffmpeg_rc < 0) {
    TEN_LOGD("Failed to reallocate FIFO.");
    return false;
  }

  ffmpeg_rc =
      av_audio_fifo_write(audio_fifo, reinterpret_cast<void **>(dst_channels),
                          static_cast<int>(dst_nb_samples));
  if (ffmpeg_rc < static_cast<int>(dst_nb_samples)) {
    TEN_LOGD("Failed to write audio sample to FIFO.");
    return false;
  }

  av_freep(&dst_channels[0]);
  av_freep(&dst_channels);

  return true;
}

ENCODE_STATUS muxer_t::encode_audio_frame(
    std::unique_ptr<ten::audio_frame_t> ten_audio_frame) {
  if (audio_encoder == nullptr) {
    TEN_LOGD("Must open audio stream first");
    return ENCODE_STATUS_ERROR;
  }

  if (ten_audio_frame->is_eof()) {
    TEN_LOGD("Encode EOF audio frame.");
    flush_remaining_audio_frames();
    return ENCODE_STATUS_EOF;
  }

  AVCodecParameters *encoded_stream_params = audio_stream->codecpar;

  // Lazy create necessary components (FIFO, frame, resampler)
  if (!allocate_audio_fifo(encoded_stream_params)) {
    return ENCODE_STATUS_ERROR;
  }
  if (!allocate_audio_frame(encoded_stream_params)) {
    return ENCODE_STATUS_ERROR;
  }
  if (!create_audio_converter(encoded_stream_params, *ten_audio_frame)) {
    return ENCODE_STATUS_ERROR;
  }

  // "convert" and "push" converted sample to FIFO.
  uint8_t **dst_channels = nullptr;
  int32_t dst_nb_samples = ten_audio_frame->get_samples_per_channel();

  // Some of the following functions would require a 'dst_nb_samples' of 'int'
  // type, so we check the value of it so that the casting would be safe.
  TEN_ASSERT(dst_nb_samples <= INT_MAX, "Should not happen.");

  int ffmpeg_rc = av_samples_alloc_array_and_samples(
      &dst_channels, nullptr, encoded_stream_params->ch_layout.nb_channels,
      dst_nb_samples,
      static_cast<enum AVSampleFormat>(encoded_stream_params->format), 0);
  if (ffmpeg_rc < 0) {
    TEN_LOGD("Failed to allocate audio sample.");
    return ENCODE_STATUS_ERROR;
  }

  uint8_t *in[8] = {nullptr};
  ten::buf_t locked_in_buf = ten_audio_frame->lock_buf();
  in[0] = locked_in_buf.data();

  ffmpeg_rc = swr_convert(audio_converter_ctx, dst_channels, dst_nb_samples,
                          const_cast<const uint8_t **>(in),
                          ten_audio_frame->get_samples_per_channel());
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to convert audio samples: %s", err_msg);
    }

    ten_audio_frame->unlock_buf(locked_in_buf);

    return ENCODE_STATUS_ERROR;
  }

  ten_audio_frame->unlock_buf(locked_in_buf);

  ffmpeg_rc =
      av_audio_fifo_realloc(audio_fifo, av_audio_fifo_size(audio_fifo) +
                                            static_cast<int>(dst_nb_samples));
  if (ffmpeg_rc < 0) {
    TEN_LOGD("Failed to reallocate FIFO.");
    return ENCODE_STATUS_ERROR;
  }

  ffmpeg_rc =
      av_audio_fifo_write(audio_fifo, reinterpret_cast<void **>(dst_channels),
                          static_cast<int>(dst_nb_samples));
  if (ffmpeg_rc < static_cast<int>(dst_nb_samples)) {
    TEN_LOGD("Failed to write audio sample to FIFO.");
    return ENCODE_STATUS_ERROR;
  }

  av_freep(&dst_channels[0]);
  av_freep(&dst_channels);

  // "pop" frame from FIFO if there are enough samples
  AVFrame *frame = audio_frame;
  while (av_audio_fifo_size(audio_fifo) >= encoded_stream_params->frame_size) {
    if (av_audio_fifo_read(audio_fifo, reinterpret_cast<void **>(frame->data),
                           frame->nb_samples) < frame->nb_samples) {
      TEN_LOGD("Failed to read data from FIFO");
      return ENCODE_STATUS_ERROR;
    }

    frame->pts = next_audio_pts();
    next_audio_idx += frame->nb_samples;

    if (!encode_audio_frame(frame)) {
      return ENCODE_STATUS_ERROR;
    }
  }

  return ENCODE_STATUS_SUCCESS;
}

int64_t muxer_t::next_video_timing() {
  return video_stream != nullptr ? pts2ms(next_video_pts(), video_stream) : 0;
}

}  // namespace ffmpeg_extension
}  // namespace ten
