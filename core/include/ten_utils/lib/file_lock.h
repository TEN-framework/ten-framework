//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#pragma once

#include "ten_utils/ten_config.h"

/**
 * @brief Set an exclusive write lock on an open file. It's designed for
 * multi-processes rather than multi-threads. Process will block if another
 * process has locked the file.
 * @return 0 if success, otherwise failed.
 */
TEN_UTILS_API int ten_file_writew_lock(int fd);

/**
 * @brief Remove lock on an open file. If the file close or process exit, lock
 * removed too.
 * @return 0 if success, otherwise failed.
 */
TEN_UTILS_API int ten_file_unlock(int fd);
