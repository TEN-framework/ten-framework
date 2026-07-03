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

TEN_UTILS_PRIVATE_API int elf_open_debug_file_by_build_id(
    ten_backtrace_t *self, const char *build_id_data, size_t build_id_size);

TEN_UTILS_PRIVATE_API int elf_open_debug_file_by_debug_link(
    ten_backtrace_t *self, const char *filename, const char *debug_link_name,
    uint32_t debug_link_crc, ten_backtrace_on_error_func_t on_error,
    void *data);
