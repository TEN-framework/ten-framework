//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#if defined(_WIN32)
#include <Windows.h>
typedef DWORD ten_thread_key_t;
#else
#include <pthread.h>
typedef pthread_key_t ten_thread_key_t;
#endif

#define kInvalidTlsKey ((ten_thread_key_t) - 1)

/**
 * @brief Create a thread local storage key.
 * @return The key.
 */
TEN_UTILS_API ten_thread_key_t ten_thread_key_create(void);

/**
 * @brief Delete a thread local storage key.
 * @param key The key.
 */
TEN_UTILS_API void ten_thread_key_destroy(ten_thread_key_t key);

/**
 * @brief Set the value of a thread local storage key.
 * @param key The key.
 * @param value The value.
 */
TEN_UTILS_API int ten_thread_set_key(ten_thread_key_t key, void *value);

/**
 * @brief Get the value of a thread local storage key.
 * @param key The key.
 * @return The value.
 */
TEN_UTILS_API void *ten_thread_get_key(ten_thread_key_t key);
