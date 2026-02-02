//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/msg.h"

#include "ten_utils/lib/smart_ptr.h"
#include "ten_utils/macro/check.h"

/*
Cross-DLL usage: These inline functions are used in ten_runtime
(ten_runtime.dll -> ten_utils.dll)

TEN_UTILS_API is used for DLL export on Windows. Without it, error
"undefined reference" will be raised.

1. Linux/macOS:
Global symbols declared with "extern" keyword are exported by default.

2. Windows(MSVC):
If the whole project is built with MSVC, then each DLL generates and uses
its own COMDAT copy of inline functions, eliminating the need for cross-DLL
imports. However, Go always uses MinGW toolchain on Windows (CGO requires
GCC-compatible compiler), regardless of whether the TEN runtime libraries
are built with MSVC or MinGW. So we need these exported implementations
whenever we're on Windows.
But MSVC does not export inline functions even with __declspec(dllexport).
So the header declares these as TEN_UTILS_API functions (not inline), and we
provide the definitions here.

3. Windows(MinGW):
According to GNU11 standard for inline functions, the "extern" keyword
should be used in .c file instead of .h file to prevent multiple definition.
So TEN_UTILS_API, which contains "extern" keyword, should be used here in .c
file.
Another solution:
MinGW uses GNU11 standard in this project, but we can use
__attribute__((gnu_inline)) in both .c and .h file to force the "inline"
keyword to work in GNU89 standard, which is exactly the opposite way.
("extern" keyword is used in .h file to prevent multiple definition)
And then TEN_UTILS_API can be used in .h file like the other functions.
*/

#if defined(_WIN32)

#if defined(_MSC_VER)
// Windows(MSVC)

TEN_RUNTIME_API bool ten_raw_msg_is_cmd_and_result(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
  case TEN_MSG_TYPE_CMD_RESULT:
    return true;

  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

TEN_RUNTIME_API bool ten_raw_msg_is_cmd(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
    return true;

  case TEN_MSG_TYPE_CMD_RESULT:
  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

TEN_RUNTIME_API bool ten_raw_msg_is_cmd_result(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");

  switch (self->type) {
  case TEN_MSG_TYPE_CMD_RESULT:
    return true;

  case TEN_MSG_TYPE_CMD_CLOSE_APP:
  case TEN_MSG_TYPE_CMD_STOP_GRAPH:
  case TEN_MSG_TYPE_CMD_START_GRAPH:
  case TEN_MSG_TYPE_CMD_TRIGGER_LIFE_CYCLE:
  case TEN_MSG_TYPE_CMD_TIMER:
  case TEN_MSG_TYPE_CMD_TIMEOUT:
  case TEN_MSG_TYPE_CMD:
  case TEN_MSG_TYPE_DATA:
  case TEN_MSG_TYPE_VIDEO_FRAME:
  case TEN_MSG_TYPE_AUDIO_FRAME:
    return false;

  default:
    TEN_ASSERT(0, "Invalid message type %d", self->type);
    return false;
  }
}

TEN_RUNTIME_API TEN_MSG_TYPE ten_raw_msg_get_type(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return self->type;
}

TEN_RUNTIME_API ten_msg_t *ten_msg_get_raw_msg(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return (ten_msg_t *)ten_shared_ptr_get_data(self);
}

TEN_RUNTIME_API bool ten_msg_is_cmd_and_result(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd_and_result(ten_msg_get_raw_msg(self));
}

TEN_RUNTIME_API bool ten_msg_is_cmd(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd(ten_msg_get_raw_msg(self));
}

TEN_RUNTIME_API bool ten_msg_is_cmd_result(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_is_cmd_result(ten_msg_get_raw_msg(self));
}

TEN_RUNTIME_API void ten_raw_msg_set_timestamp(ten_msg_t *self,
                                               int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  self->timestamp = timestamp;
}

TEN_RUNTIME_API int64_t ten_raw_msg_get_timestamp(ten_msg_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_raw_msg_check_integrity(self), "Should not happen.");
  return self->timestamp;
}

TEN_RUNTIME_API void ten_msg_set_timestamp(ten_shared_ptr_t *self,
                                           int64_t timestamp) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  ten_raw_msg_set_timestamp(ten_msg_get_raw_msg(self), timestamp);
}

TEN_RUNTIME_API int64_t ten_msg_get_timestamp(ten_shared_ptr_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_msg_check_integrity(self), "Should not happen.");
  return ten_raw_msg_get_timestamp(ten_msg_get_raw_msg(self));
}

#else
// Windows(MinGW)
TEN_RUNTIME_API inline bool ten_raw_msg_is_cmd_and_result(ten_msg_t *self);

TEN_RUNTIME_API inline bool ten_raw_msg_is_cmd(ten_msg_t *self);

TEN_RUNTIME_API inline bool ten_raw_msg_is_cmd_result(ten_msg_t *self);

TEN_RUNTIME_API inline ten_msg_t *ten_msg_get_raw_msg(ten_shared_ptr_t *self);

TEN_RUNTIME_API inline bool ten_msg_is_cmd_and_result(ten_shared_ptr_t *self);

TEN_RUNTIME_API inline bool ten_msg_is_cmd(ten_shared_ptr_t *self);

TEN_RUNTIME_API inline bool ten_msg_is_cmd_result(ten_shared_ptr_t *self);

TEN_RUNTIME_API inline TEN_MSG_TYPE ten_raw_msg_get_type(ten_msg_t *self);

TEN_RUNTIME_API inline void ten_raw_msg_set_timestamp(ten_msg_t *self,
                                                      int64_t timestamp);

TEN_RUNTIME_API inline int64_t ten_raw_msg_get_timestamp(ten_msg_t *self);

TEN_RUNTIME_API inline void ten_msg_set_timestamp(ten_shared_ptr_t *self,
                                                  int64_t timestamp);

TEN_RUNTIME_API inline int64_t ten_msg_get_timestamp(ten_shared_ptr_t *self);
#endif

#else
// Linux/MacOS
extern inline bool ten_raw_msg_is_cmd_and_result(ten_msg_t *self);  // NOLINT

extern inline bool ten_raw_msg_is_cmd(ten_msg_t *self);  // NOLINT

extern inline bool ten_raw_msg_is_cmd_result(ten_msg_t *self);  // NOLINT

extern inline ten_msg_t *ten_msg_get_raw_msg(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd_and_result(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd(ten_shared_ptr_t *self);  // NOLINT

extern inline bool ten_msg_is_cmd_result(ten_shared_ptr_t *self);  // NOLINT

extern inline TEN_MSG_TYPE ten_raw_msg_get_type(ten_msg_t *self);  // NOLINT

extern inline void ten_raw_msg_set_timestamp(ten_msg_t *self,  // NOLINT
                                             int64_t timestamp);

extern inline int64_t ten_raw_msg_get_timestamp(ten_msg_t *self);  // NOLINT

extern inline void ten_msg_set_timestamp(ten_shared_ptr_t *self,  // NOLINT
                                         int64_t timestamp);

extern inline int64_t ten_msg_get_timestamp(ten_shared_ptr_t *self);  // NOLINT
#endif
