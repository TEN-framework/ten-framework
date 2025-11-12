//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_runtime/ten_config.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "ten_runtime/binding/cpp/detail/msg/msg.h"
#include "ten_runtime/msg/video_frame/video_frame.h"
#include "ten_utils/lib/smart_ptr.h"

namespace ten {

class extension_t;
class ten_env_t;

class video_frame_t : public msg_t {
 private:
  friend extension_t;

  // Passkey Idiom.
  struct ctor_passkey_t {
   private:
    friend video_frame_t;

    explicit ctor_passkey_t() = default;
  };

 public:
  static std::unique_ptr<video_frame_t> create(const char *name,
                                               error_t *err = nullptr) {
    if (name == nullptr || strlen(name) == 0) {
      if (err != nullptr && err->get_c_error() != nullptr) {
        ten_error_set(err->get_c_error(), TEN_ERROR_CODE_INVALID_ARGUMENT,
                      "Video frame name cannot be empty.");
      }
      return nullptr;
    }

    auto *c_frame = ten_video_frame_create(
        name, err != nullptr ? err->get_c_error() : nullptr);

    return std::make_unique<video_frame_t>(c_frame, ctor_passkey_t());
  }

  explicit video_frame_t(ten_shared_ptr_t *video_frame,
                         ctor_passkey_t /*unused*/)
      : msg_t(video_frame) {}

  ~video_frame_t() override = default;

  std::unique_ptr<video_frame_t> clone() const {
    if (c_msg == nullptr) {
      TEN_ASSERT(0, "Should not happen.");
      return nullptr;
    }

    ten_shared_ptr_t *cloned_msg = ten_msg_clone(c_msg, nullptr);
    if (cloned_msg == nullptr) {
      return nullptr;
    }

    return std::make_unique<video_frame_t>(cloned_msg, ctor_passkey_t());
  }

  int32_t get_width(error_t *err = nullptr) const {
    return ten_video_frame_get_width(c_msg);
  }
  bool set_width(int32_t width, error_t *err = nullptr) {
    return ten_video_frame_set_width(c_msg, width);
  }

  int32_t get_height(error_t *err = nullptr) const {
    return ten_video_frame_get_height(c_msg);
  }
  bool set_height(int32_t height, error_t *err = nullptr) const {
    return ten_video_frame_set_height(c_msg, height);
  }

  int64_t get_timestamp(error_t *err = nullptr) const {
    return ten_video_frame_get_timestamp(c_msg);
  }
  bool set_timestamp(int64_t timestamp, error_t *err = nullptr) const {
    return ten_video_frame_set_timestamp(c_msg, timestamp);
  }

  TEN_PIXEL_FMT get_pixel_fmt(error_t *err = nullptr) const {
    return ten_video_frame_get_pixel_fmt(c_msg);
  }
  bool set_pixel_fmt(TEN_PIXEL_FMT pixel_fmt, error_t *err = nullptr) const {
    return ten_video_frame_set_pixel_fmt(c_msg, pixel_fmt);
  }

  bool is_eof(error_t *err = nullptr) const {
    return ten_video_frame_is_eof(c_msg);
  }
  bool set_eof(bool eof, error_t *err = nullptr) {
    return ten_video_frame_set_eof(c_msg, eof);
  }

  bool alloc_buf(size_t size, error_t *err = nullptr) {
    return ten_video_frame_alloc_data(c_msg, size) != nullptr;
  }

  buf_t lock_buf(error_t *err = nullptr) const {
    if (!ten_msg_add_locked_res_buf(
            c_msg, ten_video_frame_peek_buf(c_msg)->data,
            err != nullptr ? err->get_c_error() : nullptr)) {
      return buf_t{};
    }

    buf_t result{ten_video_frame_peek_buf(c_msg)->data,
                 ten_video_frame_peek_buf(c_msg)->size};

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
  video_frame_t(const video_frame_t &other) = delete;
  video_frame_t(video_frame_t &&other) noexcept = delete;
  video_frame_t &operator=(const video_frame_t &other) = delete;
  video_frame_t &operator=(video_frame_t &&other) noexcept = delete;
  // @}

  // @{
  // Internal use only. This function is called in 'extension_t' to create C++
  // message from C message.
  explicit video_frame_t(ten_shared_ptr_t *frame) : msg_t(frame) {}
  // @}

 private:
  friend ten_env_t;

  static std::unique_ptr<video_frame_t> create(ten_shared_ptr_t *video_frame,
                                               error_t *err = nullptr) {
    return std::make_unique<video_frame_t>(video_frame, ctor_passkey_t());
  }
};

}  // namespace ten
