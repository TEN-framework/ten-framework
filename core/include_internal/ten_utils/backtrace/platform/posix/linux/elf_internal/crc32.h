//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
// This file is modified from
// https://github.com/ianlancetaylor/libbacktrace [BSD license]
//
#pragma once

#include "ten_utils/ten_config.h"

#include "include_internal/ten_utils/backtrace/backtrace.h"

TEN_UTILS_PRIVATE_API uint32_t elf_crc32(uint32_t crc, const unsigned char *buf,
                                         size_t len);

TEN_UTILS_PRIVATE_API uint32_t
elf_crc32_file(ten_backtrace_t *self, int descriptor,
               ten_backtrace_on_error_func_t on_error, void *data);
