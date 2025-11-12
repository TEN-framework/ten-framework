//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include <stddef.h>

TEN_UTILS_API void ten_sanitizer_memory_record_init(void);

TEN_UTILS_API void ten_sanitizer_memory_record_deinit(void);

TEN_UTILS_API void ten_sanitizer_memory_record_dump(void);

/**
 * @brief Malloc and record memory info.
 * @see TEN_MALLOC
 * @note Please free memory using ten_sanitizer_memory_free().
 */
TEN_UTILS_API void *ten_sanitizer_memory_malloc(size_t size,
                                                const char *file_name,
                                                uint32_t lineno,
                                                const char *func_name);

TEN_UTILS_API void *ten_sanitizer_memory_calloc(size_t cnt, size_t size,
                                                const char *file_name,
                                                uint32_t lineno,
                                                const char *func_name);

/**
 * @brief Free memory and remove the record.
 * @see ten_free
 */
TEN_UTILS_API void ten_sanitizer_memory_free(void *address);

/**
 * @brief Realloc memory and record memory info.
 * @see ten_realloc
 * @note Please free memory using ten_sanitizer_memory_free().
 */
TEN_UTILS_API void *ten_sanitizer_memory_realloc(void *addr, size_t size,
                                                 const char *file_name,
                                                 uint32_t lineno,
                                                 const char *func_name);

/**
 * @brief Duplicate string and record memory info.
 * @see ten_strdup
 * @note Please free memory using ten_sanitizer_memory_free().
 */
TEN_UTILS_API char *ten_sanitizer_memory_strdup(const char *str,
                                                const char *file_name,
                                                uint32_t lineno,
                                                const char *func_name);
