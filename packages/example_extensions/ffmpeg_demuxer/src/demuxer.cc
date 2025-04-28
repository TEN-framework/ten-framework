//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include "demuxer.h"

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#if defined(__cplusplus)
extern "C" {
#endif

#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avio.h>
#include <libavutil/avstring.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#if defined(__cplusplus)
}
#endif

#include "demuxer_thread.h"
#include "ten_runtime/binding/cpp/ten.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/lib/alloc.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

#define GET_FFMPEG_ERROR_MESSAGE(err_msg, errnum)                           \
  /* NOLINTNEXTLINE */                                                      \
  for (char err_msg[AV_ERROR_MAX_STRING_SIZE], times = 0;                   \
       get_ffmpeg_error_message(err_msg, AV_ERROR_MAX_STRING_SIZE, errnum), \
                                               times == 0;                  \
       ++times)

#define DEMUXER_OUTPUT_AUDIO_FRAME_SAMPLE_FMT AV_SAMPLE_FMT_S16
#define DEMUXER_OUTPUT_VIDEO_FRAME_PIXEL_FMT AV_PIX_FMT_RGB24

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

// This is a callback which will be called during the processing of the FFmpeg,
// and if returning a non-zero value from this callback, this will break the
// processing job of FFmpeg at that time, therefore, preventing FFmpeg from
// blocking infinitely.
//
// The primary function of this method is to prevent certain FFmpeg operations
// (such as blocking I/O) from getting stuck indefinitely due to network issues
// or inaccessible resources.
int interrupt_cb(void *p) {
  TEN_ASSERT(p, "Invalid argument.");

  auto *r = reinterpret_cast<interrupt_cb_param_t *>(p);
  if (r->last_time > 0) {
    if (time(nullptr) - r->last_time > 20) {
      // If the operation continues for more than 20 seconds, it returns a
      // non-zero value to interrupt the operation.
      return 1;
    }
  }

  return 0;
}

// Debug purpose only.
TEN_UNUSED void save_avframe(const AVFrame *avFrame) {
  FILE *fDump = fopen("decode", "ab");

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
TEN_UNUSED void save_video_frame(ten::video_frame_t &pFrame, int index) {
  FILE *pFile = nullptr;
  char szFilename[32];
  int y = 0;

  int width = pFrame.get_width();
  int height = pFrame.get_height();

  // Open file
  int written = snprintf(szFilename, 32, "frame%d.ppm", index);
  TEN_ASSERT(written > 0, "Should not happen.");

  pFile = fopen(szFilename, "wb");

  if (pFile == nullptr) {
    return;
  }

  // Write header
  (void)fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  ten::buf_t locked_buf = pFrame.lock_buf();
  for (y = 0; y < height; y++) {
    (void)fwrite(locked_buf.data() + static_cast<ptrdiff_t>(y * width * 3), 1,
                 static_cast<int64_t>(width) * 3, pFile);
  }
  pFrame.unlock_buf(locked_buf);

  // Close file
  (void)fclose(pFile);
}

}  // namespace

demuxer_t::demuxer_t(ten::ten_env_proxy_t *ten_env_proxy,
                     demuxer_thread_t *demuxer_thread)
    : demuxer_thread(demuxer_thread),
      ten_env_proxy(ten_env_proxy),
      input_format_context(nullptr),
      interrupt_cb_param(nullptr),
      video_stream_idx(-1),
      audio_stream_idx(-1),
      video_decoder_ctx(nullptr),
      audio_decoder_ctx(nullptr),
      video_decoder(nullptr),
      audio_decoder(nullptr),
      video_converter_ctx(nullptr),
      audio_converter_ctx(nullptr),
      packet(av_packet_alloc()),
      frame(av_frame_alloc()),
      rotate_degree(0),
      audio_sample_rate(0),
      audio_channel_layout_mask(0),
      audio_num_of_channels(0) {}

demuxer_t::~demuxer_t() {
  av_packet_free(&packet);
  av_frame_free(&frame);

  // The ownership of 'video_decoder' belongs to ffmpeg, and it's not necessary
  // to delete it when encountering errors.

  if (video_decoder_ctx != nullptr) {
    avcodec_free_context(&video_decoder_ctx);
  }

  // The ownership of 'audio_decoder' belongs to ffmpeg, and it's not necessary
  // to delete it when encountering errors.

  if (audio_decoder_ctx != nullptr) {
    avcodec_free_context(&audio_decoder_ctx);
  }

  if (video_converter_ctx != nullptr) {
    sws_freeContext(video_converter_ctx);
  }

  if (audio_converter_ctx != nullptr) {
    swr_free(&audio_converter_ctx);
  }

  if (input_format_context != nullptr) {
    // We need to use avformat_close_input() to close all the contexts opened by
    // avformat_open_input(), otherwise we will encounter memory leakage in
    // some format (ex: hls).
    avformat_close_input(&input_format_context);
  }

  if (interrupt_cb_param != nullptr) {
    TEN_FREE(interrupt_cb_param);
    interrupt_cb_param = nullptr;
  }

  TEN_LOGD("Demuxer instance destructed.");
}

AVFormatContext *demuxer_t::create_input_format_context(
    const std::string &input_stream_loc) {
  TEN_ASSERT(input_stream_loc.length() > 0, "Invalid argument.");

  AVFormatContext *input_format_context = avformat_alloc_context();
  if (input_format_context == nullptr) {
    TEN_LOGE("Failed to create AVFormatContext.");
    return nullptr;
  }

  if (interrupt_cb_param == nullptr) {
    interrupt_cb_param = static_cast<interrupt_cb_param_t *>(
        TEN_MALLOC(sizeof(interrupt_cb_param_t)));
    TEN_ASSERT(interrupt_cb_param, "Failed to allocate memory.");
  }

  input_format_context->interrupt_callback.callback = interrupt_cb;
  input_format_context->interrupt_callback.opaque = interrupt_cb_param;

  AVDictionary *av_options = nullptr;

  // This value could be decreased to improve the latency.
  av_dict_set(&av_options, "analyzeduration", "1000000", 0);  // 1000 msec

  // The initial time is set to the current time, serving as the basis for
  // timeout checks.
  interrupt_cb_param->last_time = time(nullptr);

  // Open an input stream and read the header.
  // Wait for the input stream to appear.
  int ffmpeg_rc = avformat_open_input(
      &input_format_context, input_stream_loc.c_str(), nullptr, &av_options);

  av_dict_free(&av_options);

  if (ffmpeg_rc == 0) {
    TEN_LOGD("Open input stream %s successfully.", input_stream_loc.c_str());
  } else {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGW("Failed to open input stream %s: %s", input_stream_loc.c_str(),
               err_msg);
    }

    // Close the input, and the caller might try again.
    avformat_close_input(&input_format_context);
  }

  return input_format_context;
}

AVFormatContext *demuxer_t::create_input_format_context_with_retry(
    const std::string &input_stream_loc) {
  TEN_ASSERT(input_stream_loc.length() > 0, "Invalid argument.");

  AVFormatContext *input_format_context = nullptr;
  while (true) {
    input_format_context = create_input_format_context(input_stream_loc);
    if (input_format_context != nullptr) {
      // Open the input stream successfully.
      break;
    } else {
      // Does not detect any input stream, and does not create a corresponding
      // av format context yet.
      if (demuxer_thread->is_stopped()) {
        TEN_LOGW(
            "Giving up to detect any input stream, because the demuxer thread "
            "is stopped.");
        return nullptr;
      } else {
        // The demuxer thread is still running, try again to detect the input
        // stream.
      }
    }
  }

  return input_format_context;
}

bool demuxer_t::analyze_input_stream() {
  // `avformat_find_stream_info` will take `analyzeduration` time to analyze
  // the input stream, so it will increase the latency. If we can regularize
  // the input stream format, and want to minimize the latency, we can use some
  // fixed logic here instead of calling 'avformat_find_stream_info' to analyze
  // the input stream for us.
  int ffmpeg_rc = avformat_find_stream_info(input_format_context, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to find input stream info: %s", err_msg);
    }
    return false;
  }

  return true;
}

bool demuxer_t::open_input_stream(const std::string &init_input_stream_loc) {
  TEN_ASSERT(init_input_stream_loc.length() > 0, "Invalid argument.");

  if (is_av_decoder_opened()) {
    TEN_LOGD("Demuxer has already opened.");
    return true;
  }

  input_format_context =
      create_input_format_context_with_retry(init_input_stream_loc);
  if (input_format_context == nullptr) {
    return false;
  }

  input_stream_loc = init_input_stream_loc;

  if (!analyze_input_stream()) {
    return false;
  }

  open_video_decoder();
  open_audio_decoder();

  if (!is_av_decoder_opened()) {
    TEN_LOGW("Failed to find supported A/V codec for %s",
             input_stream_loc.c_str());
    return false;
  }

  TEN_LOGD("Input stream [%s] is opened.", input_stream_loc.c_str());

  return true;
}

bool demuxer_t::is_av_decoder_opened() {
  return video_decoder != nullptr || audio_decoder != nullptr;
}

AVCodecParameters *demuxer_t::get_video_decoder_params() const {
  if (input_format_context != nullptr && video_stream_idx >= 0) {
    return input_format_context->streams[video_stream_idx]->codecpar;
  } else {
    return nullptr;
  }
}

AVCodecParameters *demuxer_t::get_audio_decoder_params() const {
  if (input_format_context != nullptr && audio_stream_idx >= 0) {
    return input_format_context->streams[audio_stream_idx]->codecpar;
  } else {
    return nullptr;
  }
}

bool demuxer_t::create_audio_converter(const AVFrame *frame,
                                       uint64_t *out_channel_layout_mask,
                                       int *out_sample_rate) {
  TEN_ASSERT(frame && out_channel_layout_mask && out_sample_rate,
             "Invalid argument.");

  if (audio_converter_ctx == nullptr) {
    // Some audio codec (ex: pcm_mclaw) doesn't have channel layout setting.

    // Get the input channel layout from the received frame.
    AVChannelLayout in_layout;
    if (frame->ch_layout.order != AV_CHANNEL_ORDER_NATIVE) {
      av_channel_layout_default(&in_layout, frame->ch_layout.nb_channels);
    } else {
      in_layout = frame->ch_layout;
    }
    uint64_t in_channel_layout_mask = in_layout.u.mask;

    *out_channel_layout_mask = (audio_channel_layout_mask != 0)
                                   ? audio_channel_layout_mask
                                   : in_channel_layout_mask;

    *out_sample_rate =
        (audio_sample_rate != 0) ? audio_sample_rate : frame->sample_rate;

    audio_converter_ctx = swr_alloc();
    TEN_ASSERT(audio_converter_ctx, "Failed to create audio resampler");

    AVChannelLayout in_ch_layout;
    av_channel_layout_from_mask(&in_ch_layout, in_channel_layout_mask);
    av_opt_set_chlayout(audio_converter_ctx, "in_chlayout", &in_ch_layout, 0);

    AVChannelLayout out_ch_layout;
    av_channel_layout_from_mask(&out_ch_layout, *out_channel_layout_mask);
    av_opt_set_chlayout(audio_converter_ctx, "out_chlayout", &out_ch_layout, 0);

    av_opt_set_int(audio_converter_ctx, "in_sample_rate", frame->sample_rate,
                   0);
    av_opt_set_int(audio_converter_ctx, "out_sample_rate", *out_sample_rate, 0);

    av_opt_set_sample_fmt(audio_converter_ctx, "in_sample_fmt",
                          audio_decoder_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(audio_converter_ctx, "out_sample_fmt",
                          DEMUXER_OUTPUT_AUDIO_FRAME_SAMPLE_FMT, 0);

    int swr_init_rc = swr_init(audio_converter_ctx);
    if (swr_init_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, swr_init_rc) {
        TEN_LOGD("Failed to initialize resampler: %s", err_msg);
      }
      return false;
    }
  }

  return true;
}

std::unique_ptr<ten::audio_frame_t> demuxer_t::to_ten_audio_frame(
    const AVFrame *frame) {
  TEN_ASSERT(frame, "Invalid argument.");

  uint64_t out_channel_layout = 0;
  int out_sample_rate = 0;

  if (!create_audio_converter(frame, &out_channel_layout, &out_sample_rate)) {
    TEN_ASSERT(0, "Should not happen.");
    return nullptr;
  }

  auto audio_frame = ten::audio_frame_t::create("audio_frame");

  // Allocate memory for each audio channel.
  int dst_channels = frame->ch_layout.nb_channels;

  auto buf_size =
      frame->nb_samples * dst_channels *
      av_get_bytes_per_sample(DEMUXER_OUTPUT_AUDIO_FRAME_SAMPLE_FMT);
  audio_frame->alloc_buf(buf_size);

  // Convert this audio frame to the desired audio format.
  uint8_t *out[8] = {nullptr};
  ten::buf_t locked_buf = audio_frame->lock_buf();
  out[0] = locked_buf.data();

  // The amount of the converted samples might be less than the expected,
  // because they might be queued in the swr.
  int converted_audio_samples_cnt =
      swr_convert(audio_converter_ctx, out, frame->nb_samples,
                  const_cast<const uint8_t **>(frame->data),  // NOLINT
                  frame->nb_samples);
  if (converted_audio_samples_cnt < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, converted_audio_samples_cnt) {
      TEN_LOGD("Failed to convert audio samples: %s", err_msg);
    }

    audio_frame->unlock_buf(locked_buf);

    return nullptr;
  }

  audio_frame->unlock_buf(locked_buf);

  audio_frame->set_data_fmt(TEN_AUDIO_FRAME_DATA_FMT_INTERLEAVE);
  audio_frame->set_bytes_per_sample(
      av_get_bytes_per_sample(DEMUXER_OUTPUT_AUDIO_FRAME_SAMPLE_FMT));
  audio_frame->set_sample_rate(out_sample_rate);
  audio_frame->set_channel_layout(out_channel_layout);
  audio_frame->set_number_of_channels(frame->ch_layout.nb_channels);
  audio_frame->set_samples_per_channel(converted_audio_samples_cnt);

  AVRational time_base =
      input_format_context->streams[audio_stream_idx]->time_base;
  int64_t start_time =
      input_format_context->streams[audio_stream_idx]->start_time;

  // `best_effort_timestamp` is the timestamp provided by FFmpeg for a frame,
  // used to indicate the frame's position in the media stream (expressed in the
  // time base `time_base`).
  if (frame->best_effort_timestamp < start_time) {
    TEN_LOGD("Audio timestamp=%" PRId64 " < start_time=%" PRId64 "!",
             frame->best_effort_timestamp, start_time);
  }

  audio_frame->set_timestamp(av_rescale(
      // Subtract the stream's start time (`start_time`) from the frame's
      // timestamp to normalize the timestamp as an offset from the beginning of
      // the stream.
      frame->best_effort_timestamp - start_time,
      // Scale the numerator of the time base by 1000 to convert the result into
      // milliseconds.
      static_cast<int64_t>(time_base.num) * 1000, time_base.den));

  return audio_frame;
}

bool demuxer_t::create_video_converter(int width, int height) {
  if (video_converter_ctx == nullptr) {
    video_converter_ctx =
        sws_getContext(width, height, video_decoder_ctx->pix_fmt, width, height,
                       DEMUXER_OUTPUT_VIDEO_FRAME_PIXEL_FMT, SWS_POINT, nullptr,
                       nullptr, nullptr);
    if (video_converter_ctx == nullptr) {
      TEN_LOGD("Failed to create converter context for video frame.");
      return false;
    }
  }

  return true;
}

std::unique_ptr<ten::video_frame_t> demuxer_t::to_ten_video_frame(
    const AVFrame *frame) {
  TEN_ASSERT(frame, "Invalid argument.");

  int frame_width = frame->width;
  int frame_height = frame->height;

  AVRational video_time_base =
      input_format_context->streams[video_stream_idx]->time_base;
  int64_t video_start_time =
      input_format_context->streams[video_stream_idx]->start_time;

  if (frame->best_effort_timestamp < video_start_time) {
    TEN_LOGI("Video timestamp=%" PRId64 " < start_time=%" PRId64 "!",
             frame->best_effort_timestamp, video_start_time);
  }

  auto ten_video_frame = ten::video_frame_t::create("video_frame");

  if (frame->format == AV_PIX_FMT_YUV420P ||
      frame->format == AV_PIX_FMT_YUVJ420P) {
    ten_video_frame->set_pixel_fmt(TEN_PIXEL_FMT_I420);
    int size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, frame_width,
                                        frame_height, 16);

    ten_video_frame->set_width(frame_width);
    ten_video_frame->set_height(frame_height);
    ten_video_frame->set_timestamp(av_rescale(
        frame->best_effort_timestamp - video_start_time,
        static_cast<int64_t>(video_time_base.num) * 1000, video_time_base.den));

    ten_video_frame->alloc_buf(size + 32);
    ten::buf_t locked_buf = ten_video_frame->lock_buf();

    auto *y_data = locked_buf.data();
    auto *u_data = y_data + static_cast<ptrdiff_t>(frame_width * frame_height);
    auto *v_data = u_data + (frame_width * frame_height / 4);

    av_image_copy_plane(y_data, frame_width, frame->data[0], frame->linesize[0],
                        frame_width, frame_height);
    av_image_copy_plane(u_data, frame_width / 2, frame->data[1],
                        frame->linesize[1], frame_width / 2, frame_height / 2);
    av_image_copy_plane(v_data, frame_width / 2, frame->data[2],
                        frame->linesize[2], frame_width / 2, frame_height / 2);

    ten_video_frame->unlock_buf(locked_buf);
  } else if (frame->format == AV_PIX_FMT_RGB24) {
    ten_video_frame->set_pixel_fmt(TEN_PIXEL_FMT_RGB24);
    ten_video_frame->set_width(frame_width);
    ten_video_frame->set_height(frame_height);
    ten_video_frame->set_timestamp(av_rescale(
        frame->best_effort_timestamp - video_start_time,
        static_cast<int64_t>(video_time_base.num) * 1000, video_time_base.den));

    ten_video_frame->alloc_buf(
        (static_cast<int64_t>(frame_width) * frame_height * 3) + 32);

    ten::buf_t locked_buf = ten_video_frame->lock_buf();

    av_image_copy_plane(locked_buf.data(), frame_width * 3, frame->data[0],
                        frame->linesize[0], frame_width * 3, frame_height);

    ten_video_frame->unlock_buf(locked_buf);
  } else {
    TEN_LOGD("Input video frame format(%d) is not YUV420P(%d) nor RGB24(%d)",
             frame->format, AV_PIX_FMT_YUV420P, AV_PIX_FMT_RGB24);
  }

  return ten_video_frame;
}

bool demuxer_t::decode_next_video_packet(DECODE_STATUS &decode_status) {
  int ffmpeg_rc = avcodec_send_packet(video_decoder_ctx, packet);
  if (ffmpeg_rc != 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGD("Failed to decode a video packet: %s", err_msg);
    }

    decode_status = DECODE_STATUS_ERROR;
    return false;
  }

  ffmpeg_rc = avcodec_receive_frame(video_decoder_ctx, frame);
  if (ffmpeg_rc == AVERROR(EAGAIN)) {
    TEN_LOGD("Need more data to decode a video frame.");
    return true;
  } else if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGW("Failed to decode a video frame: %s", err_msg);
    }

    decode_status = DECODE_STATUS_ERROR;
    return false;
  } else {
    auto video_frame = to_ten_video_frame(frame);
    if (video_frame != nullptr) {
      // DEBUG
      // save_img_frame(video_frame, frame_->pts);

      auto video_frame_shared =
          std::make_shared<std::unique_ptr<ten::video_frame_t>>(
              std::move(video_frame));

      ten_env_proxy->notify([video_frame_shared](ten::ten_env_t &ten_env) {
        ten_env.send_video_frame(std::move(*video_frame_shared));
      });
    }

    decode_status = DECODE_STATUS_SUCCESS;
    return false;
  }
}

bool demuxer_t::decode_next_audio_packet(DECODE_STATUS &decode_status) {
  int ffmpeg_rc = avcodec_send_packet(audio_decoder_ctx, packet);

  // Skip invalid mp3 packet/frame.
  if ((AV_CODEC_ID_MP3 == audio_decoder_ctx->codec_id) &&
      ffmpeg_rc == AVERROR_INVALIDDATA) {
    TEN_LOGD("mp3 header is missing and lookup next packet.");
    return true;
  } else if (ffmpeg_rc != 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to decode audio packet: %s", err_msg);
    }

    decode_status = DECODE_STATUS_ERROR;
    return false;
  }

  // It's possible that there is more than one frame in a packet, so the
  // following while loop is used to handle all these frames in one packet.
  while (true) {
    ffmpeg_rc = avcodec_receive_frame(audio_decoder_ctx, frame);

    if (ffmpeg_rc == AVERROR(EAGAIN)) {
      TEN_LOGD("Need more data to decode audio frame");
      return true;
    } else if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGE("Failed to decode audio frame: %s", err_msg);
      }

      decode_status = DECODE_STATUS_ERROR;
      return false;
    } else {
      auto audio_frame = to_ten_audio_frame(frame);
      if (audio_frame != nullptr) {
        auto audio_frame_shared =
            std::make_shared<std::unique_ptr<ten::audio_frame_t>>(
                std::move(audio_frame));

        ten_env_proxy->notify([audio_frame_shared](ten::ten_env_t &ten_env) {
          ten_env.send_audio_frame(std::move(*audio_frame_shared));
        });
      }

      decode_status = DECODE_STATUS_SUCCESS;
      return false;
    }
  }
}

DECODE_STATUS demuxer_t::decode_next_packet() {
  if (!is_av_decoder_opened()) {
    TEN_LOGD("Must open stream first.");
    return DECODE_STATUS_ERROR;
  }

  DECODE_STATUS decode_status = DECODE_STATUS_SUCCESS;

  while (true) {
    // Discard the previous handling packet.
    av_packet_unref(packet);

    interrupt_cb_param->last_time = time(nullptr);
    int ffmpeg_rc = av_read_frame(input_format_context, packet);
    if (ffmpeg_rc < 0) {
      if (ffmpeg_rc == AVERROR_EOF) {
        flush_remaining_video_frames();
        flush_remaining_audio_frames();
        return DECODE_STATUS_EOF;
      } else {
        GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
          TEN_LOGE("Failed to get frame from input: %s", err_msg);
        }
        return DECODE_STATUS_ERROR;
      }
    }

    if (packet->stream_index == video_stream_idx) {
      if (!decode_next_video_packet(decode_status)) {
        break;
      }
    } else if (packet->stream_index == audio_stream_idx) {
      if (!decode_next_audio_packet(decode_status)) {
        break;
      }
    }
  }

  return decode_status;
}

void demuxer_t::flush_remaining_audio_frames() {
  int ffmpeg_rc = avcodec_send_packet(audio_decoder_ctx, nullptr);

  // Skip invalid mp3 packet/frame.
  if ((AV_CODEC_ID_MP3 == audio_decoder_ctx->codec_id) &&
      ffmpeg_rc == AVERROR_INVALIDDATA) {
    TEN_LOGD("mp3 header is missing and lookup next packet.");
    return;
  } else if (ffmpeg_rc != 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to decode audio packet when flushing: %s", err_msg);
    }
    return;
  }

  while (ffmpeg_rc >= 0) {
    ffmpeg_rc = avcodec_receive_frame(audio_decoder_ctx, frame);

    if (ffmpeg_rc == AVERROR(EAGAIN)) {
      TEN_LOGD("Need more data to decode audio frame when flushing");
      ffmpeg_rc = 0;
      continue;
    } else if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGE("Failed to decode audio frame when flushing: %s", err_msg);
      }
      return;
    } else {
      auto audio_frame = to_ten_audio_frame(frame);
      if (audio_frame != nullptr) {
        auto audio_frame_shared =
            std::make_shared<std::unique_ptr<ten::audio_frame_t>>(
                std::move(audio_frame));

        ten_env_proxy->notify([audio_frame_shared](ten::ten_env_t &ten_env) {
          ten_env.send_audio_frame(std::move(*audio_frame_shared));
        });
      }
    }
  }
}

void demuxer_t::flush_remaining_video_frames() {
  int ffmpeg_rc = avcodec_send_packet(video_decoder_ctx, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to decode a video when flushing packet: %s", err_msg);
    }
    return;
  }

  while (ffmpeg_rc >= 0) {
    ffmpeg_rc = avcodec_receive_frame(video_decoder_ctx, frame);
    if (ffmpeg_rc == AVERROR(EAGAIN)) {
      TEN_LOGD("Need more data to decode a video frame when flushing.");
      ffmpeg_rc = 0;
      continue;
    } else if (ffmpeg_rc < 0) {
      GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
        TEN_LOGW("Failed to decode a video frame when flushing: %s", err_msg);
      }
      return;
    } else {
      auto video_frame = to_ten_video_frame(frame);
      if (video_frame != nullptr) {
        auto video_frame_shared =
            std::make_shared<std::unique_ptr<ten::video_frame_t>>(
                std::move(video_frame));

        ten_env_proxy->notify([video_frame_shared](ten::ten_env_t &ten_env) {
          ten_env.send_video_frame(std::move(*video_frame_shared));
        });
      }
    }
  }
}

void demuxer_t::dump_video_info() {
  av_dump_format(input_format_context, video_stream_idx,
                 input_stream_loc.c_str(), 0);

  TEN_LOGD("v:width           %d", video_decoder_ctx->width);
  TEN_LOGD("v:height          %d", video_decoder_ctx->height);
  TEN_LOGD("v:time_base:      %d/%d",
           input_format_context->streams[video_stream_idx]->time_base.num,
           input_format_context->streams[video_stream_idx]->time_base.den);
  TEN_LOGD("v:start_time:     %" PRId64,
           input_format_context->streams[video_stream_idx]->start_time);
  TEN_LOGD("v:duration:       %" PRId64,
           input_format_context->streams[video_stream_idx]->duration);
  TEN_LOGD("v:nb_frames:      %" PRId64,
           input_format_context->streams[video_stream_idx]->nb_frames);
  TEN_LOGD("v:avg_frame_rate: %d/%d",
           input_format_context->streams[video_stream_idx]->avg_frame_rate.num,
           input_format_context->streams[video_stream_idx]->avg_frame_rate.den);
}

void demuxer_t::dump_audio_info() {
  av_dump_format(input_format_context, audio_stream_idx,
                 input_stream_loc.c_str(), 0);

  TEN_LOGD("a:time_base:      %d/%d",
           input_format_context->streams[audio_stream_idx]->time_base.num,
           input_format_context->streams[audio_stream_idx]->time_base.den);
  TEN_LOGD("a:start_time:     %" PRId64,
           input_format_context->streams[audio_stream_idx]->start_time);
  TEN_LOGD("a:duration:       %" PRId64,
           input_format_context->streams[audio_stream_idx]->duration);
  TEN_LOGD("a:nb_frames:      %" PRId64,
           input_format_context->streams[audio_stream_idx]->nb_frames);
}

int demuxer_t::video_width() const {
  if (video_decoder_ctx != nullptr) {
    if (rotate_degree == 0 || rotate_degree == 180) {
      return video_decoder_ctx->width;
    } else {
      return video_decoder_ctx->height;
    }
  } else {
    return 0;
  }
}

int demuxer_t::video_height() const {
  if (video_decoder_ctx != nullptr) {
    if (rotate_degree == 0 || rotate_degree == 180) {
      return video_decoder_ctx->height;
    } else {
      return video_decoder_ctx->width;
    }
  } else {
    return 0;
  }
}

int64_t demuxer_t::video_bit_rate() const {
  return video_decoder_ctx != nullptr ? video_decoder_ctx->bit_rate : 0;
}

AVRational demuxer_t::video_frame_rate() const {
  if ((input_format_context != nullptr) && video_stream_idx >= 0) {
    return input_format_context->streams[video_stream_idx]->avg_frame_rate;
  } else {
    return (AVRational){0, 1};
  }
}

AVRational demuxer_t::video_time_base() const {
  if ((input_format_context != nullptr) && video_stream_idx >= 0) {
    return input_format_context->streams[video_stream_idx]->time_base;
  } else {
    return (AVRational){0, 1};
  }
}

AVRational demuxer_t::audio_time_base() const {
  if ((input_format_context != nullptr) && audio_stream_idx >= 0) {
    return input_format_context->streams[audio_stream_idx]->time_base;
  } else {
    return (AVRational){0, 1};
  }
}

int64_t demuxer_t::number_of_video_frames() const {
  if ((input_format_context != nullptr) && video_stream_idx >= 0) {
    return input_format_context->streams[video_stream_idx]->nb_frames;
  } else {
    return 0;
  }
}

void demuxer_t::open_video_decoder() {
  TEN_ASSERT(input_format_context, "Invalid argument.");

  int idx = av_find_best_stream(input_format_context, AVMEDIA_TYPE_VIDEO, -1,
                                -1, &video_decoder, 0);
  if (idx < 0) {
    TEN_LOGW(
        "Failed to create video decoder, because video input stream not "
        "found.");
    return;
  }

  video_stream_idx = idx;

  if (video_decoder == nullptr) {
    TEN_LOGE(
        "Video input stream is found, but failed to create input video "
        "decoder, it might because the input video codec is not supported.");
    return;
  }

  AVCodecParameters *params = get_video_decoder_params();
  TEN_ASSERT(params, "Invalid argument.");

  video_decoder_ctx = avcodec_alloc_context3(video_decoder);
  if (video_decoder_ctx == nullptr) {
    TEN_LOGE("Failed to create video decoder context.");
    return;
  }

  if (avcodec_parameters_to_context(video_decoder_ctx, params) < 0) {
    TEN_LOGE("Failed to setup video decoder context parameters.");
    return;
  }

  int ffmpeg_rc = avcodec_open2(video_decoder_ctx, video_decoder, nullptr);
  if (ffmpeg_rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, ffmpeg_rc) {
      TEN_LOGE("Failed to bind video decoder to video decoder context: %s",
               err_msg);
    }
    return;
  }

  dump_video_info();  // For debug.

  TEN_LOGD("Successfully open '%s' video decoder for input stream %d",
           video_decoder->name, video_stream_idx);

  // Check metadata for rotation.
  AVDictionary *metadata =
      input_format_context->streams[video_stream_idx]->metadata;
  AVDictionaryEntry *tag =
      av_dict_get(metadata, "", nullptr, AV_DICT_IGNORE_SUFFIX);
  while (tag != nullptr) {
    TEN_LOGD("metadata: %s = %s", tag->key, tag->value);

    if (strcmp(tag->key, "rotate") == 0) {
      rotate_degree = static_cast<int>(strtol(tag->value, nullptr, 10));
      TEN_LOGD("Found rotate = %d in video stream.", rotate_degree);
    }

    tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX);
  }
}

void demuxer_t::open_audio_decoder() {
  int idx = av_find_best_stream(input_format_context, AVMEDIA_TYPE_AUDIO, -1,
                                -1, &audio_decoder, 0);
  if (idx < 0) {
    TEN_LOGW(
        "Failed to create audio decoder, because audio input stream not "
        "found.");
    return;
  }

  audio_stream_idx = idx;

  if (audio_decoder == nullptr) {
    TEN_LOGE(
        "Audio input stream is found, but failed to create input audio "
        "decoder, it might because the input audio codec is not supported.");
    return;
  }

  AVCodecParameters *params = get_audio_decoder_params();
  audio_decoder_ctx = avcodec_alloc_context3(audio_decoder);
  if (audio_decoder_ctx == nullptr) {
    TEN_LOGD("Failed to create audio decoder context.");
    return;
  }

  if (avcodec_parameters_to_context(audio_decoder_ctx, params) < 0) {
    TEN_LOGD("Failed to setup audio decoder parameters.");
    return;
  }

  int rc = avcodec_open2(audio_decoder_ctx, audio_decoder, nullptr);
  if (rc < 0) {
    GET_FFMPEG_ERROR_MESSAGE(err_msg, rc) {
      TEN_LOGE("Failed to bind audio decoder to audio decoder context: %s",
               err_msg);
    }
    return;
  }

  dump_audio_info();  // For debug.

  TEN_LOGD("Successfully open '%s' audio decoder for input stream %d",
           audio_decoder->name, audio_stream_idx);

  // Use the metadata of the audio stream as the unified audio format output by
  // the demuxer.
  audio_sample_rate = params->sample_rate;
  audio_num_of_channels = params->ch_layout.nb_channels;

  if (params->ch_layout.order != AV_CHANNEL_ORDER_NATIVE) {
    // Some audio codec (e.g., pcm_mulaw) doesn't have channel layout setting.
    AVChannelLayout default_layout;
    av_channel_layout_default(&default_layout, params->ch_layout.nb_channels);

    audio_channel_layout_mask = default_layout.u.mask;
  } else {
    audio_channel_layout_mask = params->ch_layout.u.mask;
  }
}

}  // namespace ffmpeg_extension
}  // namespace ten
