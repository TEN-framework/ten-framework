//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

#include "ten_utils/lib/string.h"

/**
 * @brief Dynamicly load a module.
 * @param name The name of the module to load.
 * @param as_local If true, the module will be loaded as RTLD_LOCAL, RTLD_GLOBAL
 *                 otherwise
 * @return The handle of the loaded module, or NULL on failure.
 * @note On iOS and Android, this function do nothing and will assert your app
 *       in debug mode.
 */
TEN_UTILS_API void *ten_module_load(const ten_string_t *name, int as_local);

/**
 * @brief Unload a module.
 * @param handle The handle of the module to unload.
 * @return 0 on success, or -1 on failure.
 */
TEN_UTILS_API int ten_module_close(void *handle);

TEN_UTILS_API void *ten_module_get_symbol(void *handle,
                                          const char *symbol_name);
