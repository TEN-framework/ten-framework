//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <cstddef>

#include "ten_utils/lib/buf.h"

namespace ten {

class value_t;
class data_t;
class video_frame_t;
class audio_frame_t;
class msg_t;

// The concept of buf_t is simple: if a memory buffer is passed in during
// construction, then buf_t will not own that memory buffer because buf_t does
// not know how to release it. Using any memory release API in buf_t's
// destructor could potentially mismatch with the API the user originally used
// to create that memory buffer. Therefore, if buf_t's constructor parameters
// are void* and size_t, the behavior is not owning that memory buffer, as the
// destructor would not know how to free/delete/release that buffer. Conversely,
// if buf_t's constructor parameter is just a size, it implies that buf_t will
// internally create the memory buffer, and in this case, buf_t will have
// ownership of that memory buffer because buf_t knows how to release it. On the
// other hand, if the user were to release the buffer themselves, it would
// result in a similar mismatch between creation and release APIs.
class buf_t {
 public:
  buf_t() : buf{TEN_BUF_STATIC_INIT_OWNED} {}

  explicit buf_t(size_t size) { ten_buf_init_with_owned_data(&buf, size); }

  buf_t(uint8_t *data, size_t size) {
    ten_buf_init_with_unowned_data(&buf, data, size);
  }

  // Copy constructor.
  buf_t(const buf_t &other) {
    ten_buf_init_with_copying_data(&buf, other.buf.data, other.buf.size);
  };

  // Move constructor.
  buf_t(buf_t &&other) noexcept : buf_t() { ten_buf_move(&buf, &other.buf); };

  ~buf_t() { ten_buf_deinit(&buf); }

  // @{
  buf_t &operator=(const buf_t &cmd) = delete;
  buf_t &operator=(buf_t &&cmd) = delete;
  // @}

  uint8_t *data() const { return buf.data; }
  size_t size() const { return buf.size; }

 private:
  friend class value_t;
  friend class data_t;
  friend class video_frame_t;
  friend class audio_frame_t;
  friend class msg_t;

  explicit buf_t(const ten_buf_t *buf) : buf{*buf} {}

  ten_buf_t buf{};
};

}  // namespace ten
