//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/addon/addon_autoload.h"

#include "include_internal/ten_runtime/addon/common/common.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/ten_env/ten_env.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/macro/memory.h"

#if defined(OS_LINUX)
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#if defined(OS_WINDOWS)
#include <windows.h>
#endif

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/addon/addon_manager.h"
#include "include_internal/ten_runtime/addon_loader/addon_loader.h"
#include "include_internal/ten_runtime/metadata/manifest.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/addon/addon.h"
#include "ten_runtime/common/error_code.h"
#include "ten_utils/container/list.h"
#include "ten_utils/lib/error.h"
#include "ten_utils/lib/module.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"

/**
 * @brief
 * - For PC platforms, we can load all extensions dynamically using
 *   dlopen/LoadLibrary.
 *
 * - For iOS, it is not allowed to load addon libraries in runtime, so "load
 *   dylib" will be done by application, and that application will mark the
 *   library dependency in _compile_time_.
 *
 * - For Android, the only "safe" way to load addon libraries is by using
 *   System.load("xxx") function from Java.
 *
 * Neither Android nor iOS support unload library.
 */
#if defined(OS_MACOS) || defined(OS_LINUX) || defined(OS_WINDOWS)

typedef struct ten_addon_loader_load_specific_addon_ctx_t {
  size_t addon_loader_count;
  size_t on_load_done_count;

  ten_addon_try_load_specific_addon_using_all_addon_loaders_func_t cb;
  void *cb_data;

  ten_env_t *ten_env;
} ten_addon_loader_load_specific_addon_ctx_t;

typedef struct ten_addon_load_all_extensions_from_app_base_dir_ctx_t {
  size_t addon_count;
  size_t loaded_addon_count;

  ten_addon_load_all_extensions_from_app_base_dir_func_t cb;
  void *cb_data;
} ten_addon_load_all_extensions_from_app_base_dir_ctx_t;

static bool load_all_dynamic_libraries_under_path(const char *path) {
  TEN_ASSERT(path, "Invalid argument.");

  bool load_at_least_one = false;

  ten_dir_fd_t *dir = NULL;
  ten_path_itor_t *itor = NULL;
  ten_string_t *file_path = NULL;
  ten_string_t *short_name = NULL;

  if (!path || !strlen(path)) {
    TEN_LOGE("Failed to load dynamic library: invalid parameter.");
    goto done;
  }

  dir = ten_path_open_dir(path);
  if (!dir) {
    TEN_LOGE("Failed to open directory: %s", path);
    goto done;
  }

  itor = ten_path_get_first(dir);
  while (itor) {
    short_name = ten_path_itor_get_name(itor);
    if (!short_name) {
      TEN_LOGE("Failed to get short name under path: %s", path);
      goto continue_loop;
    }

    if (ten_string_is_equal_c_str(short_name, ".")) {
      goto continue_loop;
    }

    if (ten_string_is_equal_c_str(short_name, "..")) {
      goto continue_loop;
    }

    file_path = ten_path_itor_get_full_name(itor);
    if (!file_path || !ten_path_is_shared_library(file_path)) {
      goto continue_loop;
    }

    if (ten_path_to_system_flavor(file_path) != 0) {
      TEN_LOGE("Failed to convert path to system flavor: %s",
               ten_string_get_raw_str(file_path));
      goto continue_loop;
    }

    void *module_handle = ten_module_load(file_path, 1);
    if (!module_handle) {
      TEN_LOGE("Failed to load module: %s", ten_string_get_raw_str(file_path));
      goto continue_loop;
    }

    load_at_least_one = true;

    TEN_LOGD("Loaded module: %s", ten_string_get_raw_str(file_path));

  continue_loop:
    if (file_path) {
      ten_string_destroy(file_path);
      file_path = NULL;
    }

    if (short_name) {
      ten_string_destroy(short_name);
      short_name = NULL;
    }

    itor = ten_path_get_next(itor);
  }

done:
  if (dir) {
    ten_path_close_dir(dir);
  }

  return load_at_least_one;
}

static void ten_addon_load_from_base_dir(const char *path) {
  ten_string_t lib_dir;
  ten_string_init_formatted(&lib_dir, "%s", path);

  if (!ten_path_is_dir(&lib_dir)) {
    goto done;
  }

  ten_string_append_formatted(&lib_dir, "/lib");
  if (ten_path_to_system_flavor(&lib_dir) != 0) {
    TEN_LOGE("Failed to convert path to system flavor: %s",
             ten_string_get_raw_str(&lib_dir));
    goto done;
  }

  if (!ten_path_exists(ten_string_get_raw_str(&lib_dir)) ||
      !ten_path_is_dir(&lib_dir)) {
    TEN_LOGD("The dynamic library path(%s) does not exist.",
             ten_string_get_raw_str(&lib_dir));
    goto done;
  }

  // Load self first.
  load_all_dynamic_libraries_under_path(ten_string_get_raw_str(&lib_dir));

done:
  ten_string_deinit(&lib_dir);
}

static void load_all_dynamic_libraries(ten_app_t *app,
                                       TEN_ADDON_TYPE addon_type,
                                       const char *path) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  ten_string_t *cur = NULL;
  ten_string_t *short_name = NULL;
  ten_string_t *self = NULL;
  ten_string_t *sub = NULL;
  ten_dir_fd_t *dir = NULL;
  ten_path_itor_t *itor = NULL;

  if (!path || !strlen(path)) {
    goto done;
  }

  dir = ten_path_open_dir(path);
  if (!dir) {
    TEN_LOGE("Failed to open directory: %s", path);
    goto done;
  }

  itor = ten_path_get_first(dir);
  while (itor) {
    short_name = ten_path_itor_get_name(itor);
    if (!short_name) {
      TEN_LOGE("Failed to get short name under path: %s", path);
      goto continue_loop;
    }

    if (ten_addon_store_find_by_type(app, addon_type,
                                     ten_string_get_raw_str(short_name))) {
      // This addon has already been loaded, so it should not be loaded again to
      // avoid re-executing any side effects during the loading process (such as
      // global variable initialization, etc.), which is typically an incorrect
      // behavior.
      goto continue_loop;
    }

    if (ten_path_is_special_dir(short_name)) {
      goto continue_loop;
    }

    cur = ten_path_itor_get_full_name(itor);
    if (!cur) {
      TEN_LOGE("Failed to get full name under path: %s", path);
      goto continue_loop;
    }

    ten_addon_load_from_base_dir(ten_string_get_raw_str(cur));

    ten_string_destroy(cur);
    cur = NULL;

  continue_loop:
    if (cur) {
      ten_string_destroy(cur);
      cur = NULL;
    }

    if (short_name) {
      ten_string_destroy(short_name);
      short_name = NULL;
    }

    if (self) {
      ten_string_destroy(self);
      self = NULL;
    }

    if (sub) {
      ten_string_destroy(sub);
      sub = NULL;
    }

    itor = ten_path_get_next(itor);
  }

done:
  if (dir) {
    ten_path_close_dir(dir);
  }
}

bool ten_addon_load_all_protocols_and_addon_loaders_from_app_base_dir(
    ten_app_t *app, TEN_UNUSED ten_error_t *err) {
  TEN_ASSERT(app, "Invalid argument.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Invalid argument.");

  const char *app_base_dir = ten_string_get_raw_str(&app->base_dir);
  TEN_ASSERT(app_base_dir, "Invalid argument.");

  bool success = true;

  // The extension is loaded dynamically and will only be loaded when it's
  // needed. Both the protocol and addon_loader are preloaded, so when the
  // app starts, it will scan these two folders and load all the addons inside
  // them.
  struct {
    TEN_ADDON_TYPE addon_type;
    const char *path;
  } folders[] = {
      {TEN_ADDON_TYPE_PROTOCOL, "/ten_packages/protocol"},
      {TEN_ADDON_TYPE_ADDON_LOADER, "/ten_packages/addon_loader"},
  };

  for (int i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
    ten_string_t module_path;
    ten_string_init_from_c_str_with_size(&module_path, app_base_dir,
                                         strlen(app_base_dir));

    do {
      ten_string_append_formatted(&module_path, folders[i].path);

      if (ten_path_to_system_flavor(&module_path) != 0) {
        TEN_LOGE("Failed to convert path to system flavor: %s",
                 ten_string_get_raw_str(&module_path));

        success = false;
        break;
      }

      // The modules (e.g., extensions/protocols) do not exist if only the TEN
      // app has been installed.
      if (ten_path_exists(ten_string_get_raw_str(&module_path))) {
        load_all_dynamic_libraries(app, folders[i].addon_type,
                                   ten_string_get_raw_str(&module_path));
      }
    } while (0);

    ten_string_deinit(&module_path);

    if (!success) {
      goto done;
    }
  }

done:
  return success;
}

static bool ten_addon_load_specific_addon_using_native_addon_loader(
    const char *app_base_dir, TEN_ADDON_TYPE addon_type, const char *addon_name,
    ten_error_t *err) {
  TEN_ASSERT(app_base_dir, "Invalid argument.");
  TEN_ASSERT(addon_name, "Addon name cannot be NULL.");

  bool success = true;
  ten_string_t addon_lib_folder_path;

  // Construct the path to the specific addon lib/ folder.
  ten_string_init_from_c_str_with_size(&addon_lib_folder_path, app_base_dir,
                                       strlen(app_base_dir));

  ten_string_append_formatted(&addon_lib_folder_path, "/ten_packages/%s/%s/lib",
                              ten_addon_type_to_string(addon_type), addon_name);
  if (ten_path_to_system_flavor(&addon_lib_folder_path) != 0) {
    TEN_LOGE("Failed to convert path to system flavor: %s",
             ten_string_get_raw_str(&addon_lib_folder_path));
    success = false;
    goto done;
  }

  if (!ten_path_exists(ten_string_get_raw_str(&addon_lib_folder_path)) ||
      !ten_path_is_dir(&addon_lib_folder_path)) {
    TEN_LOGI("Addon lib/ folder does not exist or is not a directory: %s",
             ten_string_get_raw_str(&addon_lib_folder_path));
    success = false;
    goto done;
  }

  // Load the library from the 'lib/' directory.
  success = load_all_dynamic_libraries_under_path(
      ten_string_get_raw_str(&addon_lib_folder_path));

done:
  ten_string_deinit(&addon_lib_folder_path);

  if (!success && err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "Failed to load specific addon: %s:%s",
                  ten_addon_type_to_string(addon_type), addon_name);
  }

  return success;
}

static bool ten_addon_register_specific_addon(TEN_ADDON_TYPE addon_type,
                                              const char *addon_name,
                                              void *register_ctx,
                                              ten_error_t *err) {
  TEN_ASSERT(addon_name, "Invalid argument.");

  ten_addon_manager_t *manager = ten_addon_manager_get_instance();

  bool success = ten_addon_manager_register_specific_addon(
      manager, addon_type, addon_name, register_ctx);

  if (!success && err) {
    ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                  "Failed to register specific addon: %s:%s",
                  ten_addon_type_to_string(addon_type), addon_name);
  }

  return success;
}

bool ten_addon_try_load_specific_addon_using_native_addon_loader(
    const char *app_base_dir, TEN_ADDON_TYPE addon_type, const char *addon_name,
    void *register_ctx, ten_error_t *err) {
  TEN_ASSERT(register_ctx, "Invalid argument.");

  // First, check whether the phase 2 registering function of the addon we want
  // to handle has already been added to the addon manager. If it has, proceed
  // directly with the phase 2 registration. Otherwise, start from phase 1.
  if (!ten_addon_register_specific_addon(addon_type, addon_name, register_ctx,
                                         err)) {
    // If the addon is not registered, try to load it using the native addon
    // loader (phase 1).
    bool success = ten_addon_load_specific_addon_using_native_addon_loader(
        app_base_dir, addon_type, addon_name, err);
    if (!success) {
      return false;
    }

    // If the addon is loaded successfully, try to register it again (phase 2).
    return ten_addon_register_specific_addon(addon_type, addon_name,
                                             register_ctx, err);
  }

  return true;
}

static void ten_addon_try_load_specific_addon_using_all_addon_loaders_cb(
    ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  TEN_ASSERT(ten_env_get_attach_to(ten_env) == TEN_ENV_ATTACH_TO_ADDON_LOADER,
             "Should not happen.");
  ten_addon_loader_t *addon_loader = ten_env_get_attached_addon_loader(ten_env);
  TEN_ASSERT(addon_loader, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_check_integrity(addon_loader, true),
             "Should not happen.");

  ten_addon_loader_load_specific_addon_ctx_t *ctx =
      (ten_addon_loader_load_specific_addon_ctx_t *)user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  // TODO(xilin): Check if the addon is loaded successfully. If it is, call
  // the callback. For now, we just call the callback when all addon loaders
  // have been tried.

  ctx->on_load_done_count++;

  if (ctx->on_load_done_count == ctx->addon_loader_count) {
    ctx->cb(ctx->ten_env, ctx->cb_data);

    TEN_FREE(ctx);
  }
}

void ten_addon_try_load_specific_addon_using_all_addon_loaders(
    ten_env_t *ten_env, TEN_ADDON_TYPE addon_type, const char *addon_name,
    ten_addon_try_load_specific_addon_using_all_addon_loaders_func_t cb,
    void *cb_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_app_t *app = ten_env_get_attached_app(ten_env);
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_addon_loader_singleton_store_t *addon_loader_singleton_store =
      &app->addon_loader_singleton_store;
  TEN_ASSERT(addon_loader_singleton_store, "Should not happen.");
  TEN_ASSERT(ten_addon_loader_singleton_store_check_integrity(
                 addon_loader_singleton_store, true),
             "Should not happen.");

  if (ten_list_is_empty(&addon_loader_singleton_store->store)) {
    // When there are no addon_loaders, directly call the callback.
    if (cb) {
      cb(ten_env, cb_data);
    }
    return;
  }

  ten_addon_loader_load_specific_addon_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_addon_loader_load_specific_addon_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->cb = cb;
  ctx->cb_data = cb_data;
  ctx->addon_loader_count = ten_list_size(&addon_loader_singleton_store->store);
  ctx->on_load_done_count = 0;
  ctx->ten_env = ten_env;

  ten_list_foreach (&addon_loader_singleton_store->store, iter) {
    ten_addon_loader_t *addon_loader = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon_loader, "Should not happen.");

    ten_addon_loader_load_addon(
        addon_loader, addon_type, addon_name,
        ten_addon_try_load_specific_addon_using_all_addon_loaders_cb, ctx);
  }
}

static void ten_addon_load_all_extensions_from_app_base_dir_cb(
    ten_env_t *ten_env, void *user_data) {
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");

  ten_addon_load_all_extensions_from_app_base_dir_ctx_t *ctx =
      (ten_addon_load_all_extensions_from_app_base_dir_ctx_t *)user_data;
  TEN_ASSERT(ctx, "Invalid argument.");

  ctx->loaded_addon_count++;

  if (ctx->loaded_addon_count == ctx->addon_count) {
    ctx->cb(ten_env, ctx->cb_data);

    TEN_FREE(ctx);
  }
}

bool ten_addon_load_all_extensions_from_app_base_dir(
    ten_env_t *ten_env, const char *app_base_dir,
    ten_addon_load_all_extensions_from_app_base_dir_func_t cb, void *cb_data,
    ten_error_t *err) {
  TEN_ASSERT(app_base_dir, "Invalid argument.");
  TEN_ASSERT(ten_env, "Invalid argument.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Invalid argument.");
  TEN_ASSERT(cb, "Invalid argument.");

  bool success = true;

  ten_string_t extension_folder_path;
  ten_string_init_from_c_str_with_size(&extension_folder_path, app_base_dir,
                                       strlen(app_base_dir));
  ten_string_append_formatted(&extension_folder_path,
                              "/ten_packages/extension");

  ten_list_t extension_addons_to_load_using_other_addon_loaders;
  ten_list_init(&extension_addons_to_load_using_other_addon_loaders);

  if (ten_path_to_system_flavor(&extension_folder_path) != 0) {
    TEN_LOGE("Failed to convert path to system flavor: %s",
             ten_string_get_raw_str(&extension_folder_path));
    success = false;
    if (err) {
      ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                    "Failed to convert path to system flavor: %s",
                    ten_string_get_raw_str(&extension_folder_path));
    }

    goto done;
  }

  if (ten_path_exists(ten_string_get_raw_str(&extension_folder_path))) {
    ten_dir_fd_t *dir =
        ten_path_open_dir(ten_string_get_raw_str(&extension_folder_path));
    if (!dir) {
      TEN_LOGE("Failed to open extension folder: %s",
               ten_string_get_raw_str(&extension_folder_path));
      success = false;
      if (err) {
        ten_error_set(err, TEN_ERROR_CODE_GENERIC,
                      "Failed to open extension folder: %s",
                      ten_string_get_raw_str(&extension_folder_path));
      }
      goto done;
    }

    ten_path_itor_t *itor = ten_path_get_first(dir);
    while (itor) {
      ten_string_t *short_name = ten_path_itor_get_name(itor);
      if (!short_name) {
        TEN_LOGE("Failed to get short name under extension folder: %s",
                 ten_string_get_raw_str(&extension_folder_path));
        itor = ten_path_get_next(itor);
        continue;
      }

      if (!(ten_string_is_equal_c_str(short_name, ".") ||
            ten_string_is_equal_c_str(short_name, ".."))) {
        // According to the short name, we try to load the corresponding addon
        // using native addon loader and all other addon loaders.

        // Check if the addon is already loaded.
        if (ten_addon_manager_is_addon_loaded(
                ten_addon_manager_get_instance(), TEN_ADDON_TYPE_EXTENSION,
                ten_string_get_raw_str(short_name))) {
          TEN_LOGD("Addon %s:%s is already loaded, skipping.",
                   ten_addon_type_to_string(TEN_ADDON_TYPE_EXTENSION),
                   ten_string_get_raw_str(short_name));

          ten_string_destroy(short_name);
          itor = ten_path_get_next(itor);

          continue;
        }

        // Load the addon using the native addon loader.
        bool rc = ten_addon_load_specific_addon_using_native_addon_loader(
            app_base_dir, TEN_ADDON_TYPE_EXTENSION,
            ten_string_get_raw_str(short_name), err);

        // Check if the addon is loaded successfully again.
        if (rc &&
            ten_addon_manager_is_addon_loaded(
                ten_addon_manager_get_instance(), TEN_ADDON_TYPE_EXTENSION,
                ten_string_get_raw_str(short_name))) {
          TEN_LOGD("Addon %s:%s is loaded successfully.",
                   ten_addon_type_to_string(TEN_ADDON_TYPE_EXTENSION),
                   ten_string_get_raw_str(short_name));
        } else {
          TEN_LOGD(
              "Failed to load addon %s:%s using native addon loader, will try "
              "other methods.",
              ten_addon_type_to_string(TEN_ADDON_TYPE_EXTENSION),
              ten_string_get_raw_str(short_name));

          ten_list_push_ptr_back(
              &extension_addons_to_load_using_other_addon_loaders,
              ten_string_clone(short_name),
              (ten_ptr_listnode_destroy_func_t)ten_string_destroy);
        }
      }

      ten_string_destroy(short_name);
      itor = ten_path_get_next(itor);
    }

    ten_path_close_dir(dir);
  }

  if (ten_list_is_empty(&extension_addons_to_load_using_other_addon_loaders)) {
    // Call the callback if no addon needs to be loaded using other addon
    // loaders.
    cb(ten_env, cb_data);
    goto done;
  }

  // Load the addons using all other addon loaders.
  ten_addon_load_all_extensions_from_app_base_dir_ctx_t *ctx =
      TEN_MALLOC(sizeof(ten_addon_load_all_extensions_from_app_base_dir_ctx_t));
  TEN_ASSERT(ctx, "Failed to allocate memory.");

  ctx->addon_count =
      ten_list_size(&extension_addons_to_load_using_other_addon_loaders);
  ctx->loaded_addon_count = 0;
  ctx->cb = cb;
  ctx->cb_data = cb_data;

  ten_list_foreach (&extension_addons_to_load_using_other_addon_loaders, iter) {
    ten_string_t *addon_name = ten_ptr_listnode_get(iter.node);
    TEN_ASSERT(addon_name, "Should not happen.");

    ten_addon_try_load_specific_addon_using_all_addon_loaders(
        ten_env, TEN_ADDON_TYPE_EXTENSION, ten_string_get_raw_str(addon_name),
        ten_addon_load_all_extensions_from_app_base_dir_cb, ctx);
  }

done:
  ten_string_deinit(&extension_folder_path);
  ten_list_clear(&extension_addons_to_load_using_other_addon_loaders);
  return success;
}

#endif
