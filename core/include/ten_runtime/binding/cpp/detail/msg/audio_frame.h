//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <cstddef>
#include <memory>

#include "ten_runtime/binding/cpp/detail/msg/msg.h"
#include "ten_runtime/msg/audio_frame/audio_frame.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class ten_env_t;
class extension_t;

class audio_frame_t : public msg_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend audio_frame_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  static std::unique_ptr<audio_frame_t> create(const char *name,
                                               error_t *err = nullptr) {
    if (name == nullptr || strlen(name) == 0) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "audio frame name cannot be empty.");
      }
      return nullptr;
    }

    auto *c_frame = ten_audio_frame_create(
        name, err != nullptr ? err->get_c_error() : nullptr);

    return std::make_unique<audio_frame_t>(c_frame, ctor_passkey_t());
  }

  explicit audio_frame_t(ten_shared_ptr_t *audio_frame,
                         ctor_passkey_t /*unused*/)
      : msg_t(audio_frame) {}

  ~audio_frame_t() override = default;

  std::unique_ptr<audio_frame_t> clone() const {
    if (c_msg == nullptr) {
      TEN_ASSERT(0, "Should not happen.");
      return nullptr;
    }

    ten_shared_ptr_t *cloned_msg = ten_msg_clone(c_msg, nullptr);
    if (cloned_msg == nullptr) {
      return nullptr;
    }

    return std::make_unique<audio_frame_t>(cloned_msg, ctor_passkey_t());
  }

  int64_t get_timestamp(error_t *err = nullptr) const {
    return ten_audio_frame_get_timestamp(c_msg);
  }
  bool set_timestamp(int64_t timestamp, error_t *err = nullptr) {
    return ten_audio_frame_set_timestamp(c_msg, timestamp);
  }

  int32_t get_sample_rate(error_t *err = nullptr) const {
    return ten_audio_frame_get_sample_rate(c_msg);
  }
  bool set_sample_rate(int32_t sample_rate, error_t *err = nullptr) {
    return ten_audio_frame_set_sample_rate(c_msg, sample_rate);
  }

  uint64_t get_channel_layout(error_t *err = nullptr) const {
    return ten_audio_frame_get_channel_layout(c_msg);
  }
  bool set_channel_layout(uint64_t channel_layout, error_t *err = nullptr) {
    return ten_audio_frame_set_channel_layout(c_msg, channel_layout);
  }

  int32_t get_samples_per_channel(error_t *err = nullptr) const {
    return ten_audio_frame_get_samples_per_channel(c_msg);
  }
  bool set_samples_per_channel(int32_t samples_per_channel,
                               error_t *err = nullptr) {
    return ten_audio_frame_set_samples_per_channel(c_msg, samples_per_channel);
  }

  int32_t get_bytes_per_sample(error_t *err = nullptr) const {
    return ten_audio_frame_get_bytes_per_sample(c_msg);
  }
  bool set_bytes_per_sample(int32_t size, error_t *err = nullptr) {
    return ten_audio_frame_set_bytes_per_sample(c_msg, size);
  }

  int32_t get_number_of_channels(error_t *err = nullptr) const {
    return ten_audio_frame_get_number_of_channel(c_msg);
  }
  bool set_number_of_channels(int32_t number, error_t *err = nullptr) {
    return ten_audio_frame_set_number_of_channel(c_msg, number);
  }

  TEN_AUDIO_FRAME_DATA_FMT get_data_fmt(error_t *err = nullptr) const {
    return ten_audio_frame_get_data_fmt(c_msg);
  }
  bool set_data_fmt(TEN_AUDIO_FRAME_DATA_FMT format, error_t *err = nullptr) {
    return ten_audio_frame_set_data_fmt(c_msg, format);
  }

  int32_t get_line_size(error_t *err = nullptr) const {
    return ten_audio_frame_get_line_size(c_msg);
  }
  bool set_line_size(int32_t line_size, error_t *err = nullptr) {
    return ten_audio_frame_set_line_size(c_msg, line_size);
  }

  bool is_eof(error_t *err = nullptr) const {
    return ten_audio_frame_is_eof(c_msg);
  }
  bool set_eof(bool eof, error_t *err = nullptr) {
    return ten_audio_frame_set_eof(c_msg, eof);
  }

  bool alloc_buf(size_t size, error_t *err = nullptr) {
    return ten_audio_frame_alloc_buf(c_msg, size) != nullptr;
  }

  buf_t lock_buf(error_t *err = nullptr) const {
    ten_buf_t *data = ten_audio_frame_peek_buf(c_msg);

    if (!ten_msg_add_locked_res_buf(
            c_msg, data->data, err != nullptr ? err->get_c_error() : nullptr)) {
      return buf_t{};
    }

    buf_t result{data->data, data->size};

    return result;
  }

  bool unlock_buf(buf_t &buf, error_t *err = nullptr) {
    const uint8_t *data = buf.data();
    if (!ten_msg_remove_locked_res_buf(
            c_msg, data, err != nullptr ? err->get_c_error() : nullptr)) {
      return false;
    }

    // Since the `buf` has already been given back, clearing the contents of the
    // `buf` itself not only notifies developers that this `buf` can no longer
    // be used, but also prevents it from being used incorrectly again.
    ten_buf_init_with_owned_data(&buf.buf, 0);

    return true;
  }

  // @{
  audio_frame_t(const audio_frame_t &other) = delete;
  audio_frame_t(audio_frame_t &&other) noexcept = delete;
  audio_frame_t &operator=(const audio_frame_t &other) = delete;
  audio_frame_t &operator=(audio_frame_t &&other) noexcept = delete;
  // @}

  // @{
  // Internal use only. This function is called in 'extension_t' to create C++
  // message from C message.
  explicit audio_frame_t(ten_shared_ptr_t *frame) : msg_t(frame) {}
  // @}

 private:
  friend class extension_t;
  friend ten_env_t;

  static std::unique_ptr<audio_frame_t> create(ten_shared_ptr_t *audio_frame,
                                               error_t *err = nullptr) {
    return std::make_unique<audio_frame_t>(audio_frame, ctor_passkey_t());
  }
};

}  // namespace ten
