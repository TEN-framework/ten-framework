//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/ten_config.h"

#include "include_internal/ten_runtime/common/base_dir.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "ten_utils/lib/file.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/macro/check.h"

ten_string_t *ten_find_app_base_dir(void) {
  ten_string_t *app_base_dir = NULL;

  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *ten_app_base_dir_env_var = getenv("TEN_APP_BASE_DIR");
  if (ten_app_base_dir_env_var && strlen(ten_app_base_dir_env_var) > 0) {
    app_base_dir = ten_string_create_formatted("%s", ten_app_base_dir_env_var);
    ten_path_to_system_flavor(app_base_dir);
  } else {
    // The following `ten_path_get_module_path()` function returns the base
    // directory of `libten_runtime.so`.
    //
    // Note that we can not use `ten_path_get_executable_path()` here, as the
    // actually executable in some language is not the TEN app. Ex: the
    // `ten_path_get_executable_path()` returns `/usr/bin` if we start a python
    // APP with `python3 bin/main.py`. In this case, the `/usr/bin` is the
    // location of `python3`.
    ten_string_t *module_path =
        ten_path_get_module_path((void *)ten_find_app_base_dir);
    if (!module_path) {
      TEN_LOGE(
          "Could not get app base dir from module path, using TEN_APP_BASE_DIR "
          "instead.");
      return NULL;
    }

    app_base_dir = ten_find_base_dir(ten_string_get_raw_str(module_path),
                                     TEN_STR_APP, NULL);
    ten_string_destroy(module_path);
  }

  if (!app_base_dir || ten_string_is_empty(app_base_dir)) {
    TEN_LOGW(
        "Could not get app home from module path, using TEN_APP_BASE_DIR "
        "instead.");
    return NULL;
  }

  return app_base_dir;
}

void ten_app_find_and_set_base_dir(ten_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(self, true), "Should not happen.");

  ten_string_t *app_base_dir = ten_find_app_base_dir();
  if (!app_base_dir) {
    TEN_LOGD("Failed to determine app base directory.");
    return;
  }

  ten_string_copy(&self->base_dir, app_base_dir);
  ten_string_destroy(app_base_dir);
}

const char *ten_app_get_base_dir(ten_app_t *self) {
  // TEN_NOLINTNEXTLINE(thread-check)
  // thread-check: This function might be called from other threads, ex: the
  // extension thread. And the `base_dir` is only set when starting the app, so
  // it's thread safe to read after app starts.
  TEN_ASSERT(self && ten_app_check_integrity(self, false), "Invalid argument.");

  return ten_string_get_raw_str(&self->base_dir);
}
