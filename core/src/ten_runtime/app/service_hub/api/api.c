//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#if defined(TEN_ENABLE_TEN_RUST_APIS)

#include <stddef.h>

#include "include_internal/ten_runtime/common/version.h"
#include "include_internal/ten_utils/log/log.h"

const char *ten_get_runtime_version(void) { return TEN_RUNTIME_VERSION; }

const char *ten_get_global_log_path(void) {
  return ten_log_global_get_output_file_path();
}

#endif
