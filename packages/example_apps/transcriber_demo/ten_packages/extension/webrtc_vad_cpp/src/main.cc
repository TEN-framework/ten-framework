//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ten_runtime/binding/cpp/ten.h"
#include "webrtc_vad.h"

namespace webrtc_vad_cpp {

class webrtc_vad_extension_t : public ten::extension_t {
 public:
  explicit webrtc_vad_extension_t(const char *name)
      : ten::extension_t(name), vad_handle_(nullptr), mode_(2) {}

  void on_init(ten::ten_env_t &ten_env) override {
    // Get configuration from property
    mode_ = ten_env.get_property_int32("mode");
    if (mode_ < 0 || mode_ > 3) {
      TEN_LOGE("Invalid VAD mode %d, using default mode 2", mode_);
      mode_ = 2;
    }

    // Create and initialize VAD instance
    vad_handle_ = WebRtcVad_Create();
    if (vad_handle_ == nullptr) {
      TEN_LOGE("Failed to create WebRTC VAD instance");
      ten_env.on_init_done();
      return;
    }

    if (WebRtcVad_Init(vad_handle_) != 0) {
      TEN_LOGE("Failed to initialize WebRTC VAD");
      WebRtcVad_Free(vad_handle_);
      vad_handle_ = nullptr;
      ten_env.on_init_done();
      return;
    }

    if (WebRtcVad_set_mode(vad_handle_, mode_) != 0) {
      TEN_LOGE("Failed to set VAD mode");
      WebRtcVad_Free(vad_handle_);
      vad_handle_ = nullptr;
      ten_env.on_init_done();
      return;
    }

    TEN_ENV_LOG_INFO(
        ten_env,
        ("WebRTC VAD initialized with mode " + std::to_string(mode_)).c_str());
    ten_env.on_init_done();
  }

  void on_audio_frame(ten::ten_env_t &ten_env,
                      std::unique_ptr<ten::audio_frame_t> frame) override {
    if (vad_handle_ == nullptr) {
      TEN_ENV_LOG_WARN(ten_env, "VAD not initialized, dropping audio frame");
      return;
    }

    std::string frame_name = frame->get_name();
    int32_t sample_rate = frame->get_sample_rate();
    int32_t bytes_per_sample = frame->get_bytes_per_sample();
    int32_t samples_per_channel = frame->get_samples_per_channel();
    int32_t number_of_channels = frame->get_number_of_channels();

    TEN_ENV_LOG_DEBUG(
        ten_env, ("Received audio frame: rate=" + std::to_string(sample_rate) +
                  ", bps=" + std::to_string(bytes_per_sample) +
                  ", samples=" + std::to_string(samples_per_channel) +
                  ", channels=" + std::to_string(number_of_channels))
                     .c_str());

    // Lock the buffer to access audio data
    ten::buf_t locked_buf = frame->lock_buf();

    // WebRTC VAD expects int16_t samples
    if (bytes_per_sample != 2) {
      TEN_ENV_LOG_WARN(ten_env,
                       ("VAD requires 16-bit samples, got " +
                        std::to_string(bytes_per_sample) + " bytes per sample")
                           .c_str());
      frame->unlock_buf(locked_buf);
      return;
    }

    // For multi-channel audio, use first channel only
    size_t frame_length = samples_per_channel;
    const int16_t *audio_data =
        reinterpret_cast<const int16_t *>(locked_buf.data());

    // If multi-channel, extract first channel
    std::vector<int16_t> mono_samples;
    if (number_of_channels > 1) {
      mono_samples.resize(frame_length);
      for (size_t i = 0; i < frame_length; i++) {
        mono_samples[i] = audio_data[i * number_of_channels];
      }
      audio_data = mono_samples.data();
    }

    // Validate rate and frame length
    if (WebRtcVad_ValidRateAndFrameLength(sample_rate, frame_length) != 0) {
      TEN_ENV_LOG_WARN(ten_env, ("Invalid rate/frame_length combination: " +
                                 std::to_string(sample_rate) + " Hz, " +
                                 std::to_string(frame_length) + " samples")
                                    .c_str());
      frame->unlock_buf(locked_buf);
      return;
    }

    // Process audio frame through VAD
    int vad_result =
        WebRtcVad_Process(vad_handle_, sample_rate, audio_data, frame_length);

    frame->unlock_buf(locked_buf);

    if (vad_result < 0) {
      TEN_ENV_LOG_ERROR(ten_env, "VAD processing error");
      return;
    }

    // Add VAD result as properties to the audio frame
    frame->set_property("is_speech", vad_result == 1);
    frame->set_property("frame_name", frame_name);

    TEN_ENV_LOG_DEBUG(
        ten_env, ("VAD result: is_speech=" + std::to_string(vad_result == 1) +
                  ", frame_name=" + frame_name)
                     .c_str());

    // Forward the audio frame with VAD properties for downstream processing
    ten_env.send_audio_frame(std::move(frame));
  }

  void on_deinit(ten::ten_env_t &ten_env) override {
    if (vad_handle_ != nullptr) {
      WebRtcVad_Free(vad_handle_);
      vad_handle_ = nullptr;
      TEN_ENV_LOG_INFO(ten_env, "WebRTC VAD cleaned up");
    }
    ten_env.on_deinit_done();
  }

 private:
  VadInst *vad_handle_;
  int32_t mode_;
};

}  // namespace webrtc_vad_cpp

TEN_CPP_REGISTER_ADDON_AS_EXTENSION(webrtc_vad_cpp,
                                    webrtc_vad_cpp::webrtc_vad_extension_t);
