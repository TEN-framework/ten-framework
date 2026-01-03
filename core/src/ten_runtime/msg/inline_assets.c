//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/msg/msg.h"

/*
Cross-DLL usage: These inline functions are called from packages (e.g.,
msgpack.dll -> ten_runtime.dll). For example,
packages/core_protocols/msgpack/ is compiled as msgpack.dll, and its
msg/msg.c calls ten_msg_get_raw_msg().

TEN_UTILS_API is used for DLL export on Windows(MinGW). Without it,
error "undefined reference" will be raised.

According to GNU11 standard for inline functions, the "extern" keyword
should be used in .c file instead of .h file to prevent multiple definition.
So TEN_UTILS_API, which contains "extern" keyword, should be used here in .c
file.

Why only Windows(MinGW) needs this (not Linux/macOS/MSVC):
1. Linux/macOS: Global symbols declared with "extern" keyword are exported by
default.
2. Windows(MSVC): Each DLL generates and uses its own COMDAT copy of inline
functions, eliminating the need for cross-DLL imports.

Another solution:
MinGW uses GNU11 standard in this project, but we can use
__attribute__((gnu_inline)) in both .c and .h file to force the "inline"
keyword to work in GNU89 standard, which is exactly the opposite way.
("extern" keyword is used in .h file to prevent multiple definition)
And then TEN_UTILS_API can be used in .h file like the other functions.
*/
#if defined(__MINGW32__) || defined(__MINGW64__)
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
#else
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