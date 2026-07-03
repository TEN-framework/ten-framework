//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "gtest/gtest.h"
#include "ten_runtime/binding/cpp/detail/msg/audio_frame.h"
#include "ten_runtime/binding/cpp/detail/test/env_tester.h"
#include "ten_runtime/binding/cpp/ten.h"

namespace {

class webrtc_vad_cpp_tester : public ten::extension_tester_t {
 public:
  webrtc_vad_cpp_tester() = default;
  ~webrtc_vad_cpp_tester() override = default;

  // @{
  webrtc_vad_cpp_tester(webrtc_vad_cpp_tester &other) = delete;
  webrtc_vad_cpp_tester(webrtc_vad_cpp_tester &&other) = delete;
  webrtc_vad_cpp_tester &operator=(const webrtc_vad_cpp_tester &cmd) = delete;
  webrtc_vad_cpp_tester &operator=(webrtc_vad_cpp_tester &&cmd) = delete;
  // @}

  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Generate test audio: silence -> speech -> silence
    const int sample_rate = 16000;
    const int frame_duration_ms = 20;
    const int samples_per_frame = sample_rate * frame_duration_ms / 1000;

    // Send 10 frames of silence
    for (int i = 0; i < 10; i++) {
      send_audio_frame(ten_env, samples_per_frame, sample_rate, false);
    }

    // Send 20 frames with speech-like signal (sine wave with sufficient
    // amplitude)
    for (int i = 0; i < 20; i++) {
      send_audio_frame(ten_env, samples_per_frame, sample_rate, true);
    }

    // Send 10 frames of silence
    for (int i = 0; i < 10; i++) {
      send_audio_frame(ten_env, samples_per_frame, sample_rate, false);
    }

    ten_env.on_start_done();
  }

  void on_audio_frame(ten::ten_env_tester_t &ten_env,
                      std::unique_ptr<ten::audio_frame_t> frame) override {
    std::string frame_name = frame->get_name();

    // Check if the frame has VAD properties
    auto is_speech = frame->get_property_bool("is_speech");
    auto vad_frame_name = frame->get_property_string("frame_name");

    TEN_LOGI(
        "Received audio frame with VAD result: is_speech=%d, frame_name=%s",
        is_speech, vad_frame_name.c_str());

    vad_results_.push_back(is_speech);

    // After receiving enough results, validate and stop
    if (vad_results_.size() >= 40) {
      validate_results();
      ten_env.stop_test();
    }
  }

  void on_data(ten::ten_env_tester_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    // Not used anymore, VAD results are now in audio_frame properties
  }

 private:
  void send_audio_frame(ten::ten_env_tester_t &ten_env, int samples_per_frame,
                        int sample_rate, bool is_speech) {
    auto frame = ten::audio_frame_t::create("audio_frame");

    size_t buffer_size = samples_per_frame * sizeof(int16_t);
    bool rc = frame->alloc_buf(buffer_size);
    EXPECT_EQ(rc, true);

    ten::buf_t locked_buf = frame->lock_buf();
    EXPECT_NE(locked_buf.data(), nullptr);
    EXPECT_EQ(locked_buf.size(), buffer_size);

    auto *samples = reinterpret_cast<int16_t *>(locked_buf.data());

    if (is_speech) {
      // Generate a 440 Hz sine wave with amplitude 3000 (sufficient for VAD
      // detection)
      const double frequency = 440.0;
      const double amplitude = 3000.0;
      for (int i = 0; i < samples_per_frame; i++) {
        double t = static_cast<double>(frame_count_ * samples_per_frame + i) /
                   sample_rate;
        samples[i] =
            static_cast<int16_t>(amplitude * sin(2.0 * M_PI * frequency * t));
      }
    } else {
      // Generate silence (very low amplitude noise)
      for (int i = 0; i < samples_per_frame; i++) {
        samples[i] = static_cast<int16_t>((rand() % 100) - 50);  // NOLINT
      }
    }

    frame->unlock_buf(locked_buf);

    frame->set_sample_rate(sample_rate);
    frame->set_bytes_per_sample(2);
    frame->set_samples_per_channel(samples_per_frame);
    frame->set_number_of_channels(1);
    frame->set_timestamp(frame_count_ * frame_duration_ms_);

    ten_env.send_audio_frame(std::move(frame));
    frame_count_++;
  }

  void validate_results() {
    EXPECT_GE(vad_results_.size(), 40);

    // Count speech detections in different regions
    int speech_count_silence1 = 0;
    int speech_count_speech = 0;
    int speech_count_silence2 = 0;

    for (size_t i = 0; i < 10 && i < vad_results_.size(); i++) {
      if (vad_results_[i]) {
        speech_count_silence1++;
      }
    }

    for (size_t i = 10; i < 30 && i < vad_results_.size(); i++) {
      if (vad_results_[i]) {
        speech_count_speech++;
      }
    }

    for (size_t i = 30; i < 40 && i < vad_results_.size(); i++) {
      if (vad_results_[i]) {
        speech_count_silence2++;
      }
    }

    TEN_LOGI(
        "VAD results summary: silence1=%d/10, speech=%d/20, silence2=%d/10",
        speech_count_silence1, speech_count_speech, speech_count_silence2);

    // Validate: speech section should have more detections than silence
    // sections Due to smoothing, we allow some tolerance
    EXPECT_GE(speech_count_speech, 10);  // At least 50% speech detected
    EXPECT_LE(speech_count_silence1 + speech_count_silence2,
              10);  // At most 50% false positives
  }

  std::vector<bool> vad_results_;
  int frame_count_ = 0;
  const int frame_duration_ms_ = 20;
};

}  // namespace

TEST(Test, Basic) {  // NOLINT
  auto *tester = new webrtc_vad_cpp_tester();
  tester->set_test_mode_single("webrtc_vad_cpp");
  tester->run();
  delete tester;
}
