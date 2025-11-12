//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
#include <cstdlib>

#include "gtest/gtest.h"
#include "ten_runtime/binding/cpp/detail/msg/audio_frame.h"
#include "ten_runtime/binding/cpp/detail/test/env_tester.h"
#include "ten_runtime/binding/cpp/ten.h"

namespace {

class vosk_asr_cpp_tester : public ten::extension_tester_t {
 public:
  vosk_asr_cpp_tester()
      :  // Open the audio file (make sure the audio is in PCM format with a
         // sampling rate of 16 KHz).
        test_wav_fp(fopen("./tests/test.wav", "rb")) {
    EXPECT_NE(test_wav_fp, nullptr);
  }

  ~vosk_asr_cpp_tester() override { (void)fclose(test_wav_fp); }

  // @{
  vosk_asr_cpp_tester(vosk_asr_cpp_tester &other) = delete;
  vosk_asr_cpp_tester(vosk_asr_cpp_tester &&other) = delete;
  vosk_asr_cpp_tester &operator=(const vosk_asr_cpp_tester &cmd) = delete;
  vosk_asr_cpp_tester &operator=(vosk_asr_cpp_tester &&cmd) = delete;
  // @}

  void on_start(ten::ten_env_tester_t &ten_env) override {
    // Read the audio data and perform recognition.
    const int buffer_size = 4096;
    char buffer[buffer_size];
    size_t nread = 0;

    // Continuously sending audio data in. The vosk extension will continuously
    // process the data and output the recognized text.
    while ((nread = fread(buffer, 1, buffer_size, test_wav_fp)) > 0) {
      auto frame = ten::audio_frame_t::create("recognize");

      bool rc = frame->alloc_buf(nread);
      EXPECT_EQ(rc, true);

      ten::buf_t locked_in_buf = frame->lock_buf();
      EXPECT_NE(locked_in_buf.data(), nullptr);
      EXPECT_NE(locked_in_buf.size(), 0);

      memcpy(locked_in_buf.data(), buffer, nread);

      frame->unlock_buf(locked_in_buf);

      ten_env.send_audio_frame(std::move(frame));
    }

    ten_env.on_start_done();
  }

  void on_data(ten::ten_env_tester_t &ten_env,
               std::unique_ptr<ten::data_t> data) override {
    std::string data_name = data->get_name();

    auto result = data->get_property_string("result");
    auto is_final = data->get_property_int32("is_final");

    TEN_LOGI("Received data '%s(%d)': %s", data_name.c_str(), is_final,
             result.c_str());

    if (is_final > 0) {
      ten_env.stop_test();
    }
  }

 private:
  FILE *test_wav_fp = nullptr;
};

}  // namespace

TEST(Test, Basic) {  // NOLINT
  auto *tester = new vosk_asr_cpp_tester();
  tester->set_test_mode_single("vosk_asr_cpp");
  tester->run();
  delete tester;
}
