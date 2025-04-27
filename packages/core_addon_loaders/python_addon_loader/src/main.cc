//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <cinttypes>
#include <cstring>

#include "include_internal/ten_runtime/addon/addon.h"
#include "include_internal/ten_runtime/app/metadata.h"
#include "include_internal/ten_runtime/binding/cpp/detail/addon_loader.h"
#include "include_internal/ten_runtime/binding/cpp/detail/addon_manager.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "include_internal/ten_runtime/binding/python/common.h"
#include "include_internal/ten_runtime/common/base_dir.h"
#include "include_internal/ten_runtime/common/constant_str.h"
#include "include_internal/ten_runtime/metadata/manifest.h"
#include "ten_runtime/addon/addon.h"
#include "ten_utils/container/list_str.h"
#include "ten_utils/lib/module.h"
#include "ten_utils/lib/path.h"
#include "ten_utils/lib/string.h"
#include "ten_utils/log/log.h"
#include "ten_utils/macro/check.h"

static void foo() {}

/**
 * This addon is used for those ten app whose "main" function is not written in
 * python. By putting this addon into a ten app, the python runtime can be
 * initialized and other python addons can be loaded and registered to the ten
 * world when the ten app is started.
 *
 * Time sequence:
 *
 * 0) The executable of the ten app (non-python) links with libten_runtime.
 *
 * 1) The program of the ten app (non-python) is started, with libten_runtime
 *    being loaded, which triggers this addon to be dlopen-ed.
 *
 * 2) libten_runtime will call 'ten_addon_register_extension()' synchronously,
 *    then python_addon_loader_addon_t::on_init() will be called from
 * libten_runtime.
 *
 * 3) python_addon_loader_addon_t::on_init() will handle things including
 * Py_Initialize, setup sys.path, load all python addons in the app's addon/
 * folder.
 *
 * 4) libten_runtime_python will be loaded when any python addon is loaded (due
 *    to the python code: 'import libten_runtime_python')
 *
 * 5) After all python addons are registered,
 * python_addon_loader_addon_t::on_init() will release the python GIL so that
 * other python codes can be executed in any other threads after they acquiring
 * the GIL.
 *
 * ================================================
 * What will happen if the app is a python program?
 *
 * If no special handling is done, there will be the following 2 problems:
 *
 * 1) Python prohibits importing the same module again before it has been fully
 *    imported (i.e., circular imports). And if the main program is a python
 *    program, and if the main program loads libten_runtime_python (because it
 *    might need some features in it), python addons will be loaded after
 *    libten_runtime_python is imported (because libten_runtime_python will
 *    loads libten_runtime, and libten_runtime will loop addon/ folder to
 *    load/dlopen all the _native_ addons in it, and it will load
 *    python_addon_loader, and this python_addon_loader will load all python
 * addons in addon/ folder). And if these loaded Python addons load
 *    libten_runtime_python (because they need to use its functionalities),
 *    which will create a circular import.
 *
 * 2. If the main program is a python program and it loaded this addon
 *    _synchronously_ in the python main thread (see above), then if the GIL is
 *    released in python_addon_loader_addon_t::on_init, then no more further
 * python codes can be executed normally in the python main thread.
 *
 * 3. Even though the app is not a python program, if the python
 *    multiprocessing mode is set to 'spawn', then the subprocess will be
 *    executed by a __Python__ interpreter, not the origin native executable.
 *    While if the 'libten_runtime_python' module is imported before the target
 *    function is called in subprocess (For example, if the Python module
 *    containing the target function or its parent folder's Python module
 *    imports ten_runtime_python.) (And this situation is similar to the python
 *    main situation), then libten_runtime will be loaded again, which will
 *    cause this addon to be loaded. Which results in a circular import similar
 *    to the situation described above.
 *
 * How to avoid any side effects?
 *
 * The main reason is that, theoretically, python main and python_addon_loader
 * should not be used together. However, due to some reasonable or unreasonable
 * reasons mentioned above, python main and python_addon_loader are being used
 * together. Therefore, what we need to do in this situation is to detect this
 * case and then essentially disable python_addon_loader. By checking
 * 'ten_py_is_initialized' on python_addon_loader_addon_t::on_init, we can know
 * whether the python runtime has been initialized. And the calling operation
 * here is thread safe, because if the app is not a python program, the python
 * runtime is not initialized for sure, and if the app is a python program, then
 * the python_addon_loader_addon_t::on_init will be called in the python main
 * thread and the GIL is held, so it is thread safe to call
 * 'ten_py_is_initialized'.
 */

namespace {

class python_addon_loader_t : public ten::addon_loader_t {
 public:
  explicit python_addon_loader_t(const char *name) { (void)name; };

  void on_init(ten::ten_env_t &ten_env) override {
    // Do some initializations.

    int py_initialized = ten_py_is_initialized();
    if (py_initialized != 0) {
      TEN_LOGI("[Python addon loader] Python runtime has been initialized.");
      ten_env.on_init_done();
      return;
    }

    py_init_by_self_ = true;

    // We met 'symbols not found' error when loading python modules while the
    // symbols are expected to be found in the python lib. We need to load the
    // python lib first.
    //
    // Refer to
    // https://mail.python.org/pipermail/new-bugs-announce/2008-November/003322.html?from_wecom=1
    load_python_lib();

    ten_py_initialize();

    find_app_base_dir();

    // Before loading the ten python modules (extensions), we have to complete
    // sys.path first.
    complete_sys_path();

    ten_py_run_simple_string(
        "import sys\n"
        "print(sys.path)\n");

    const auto *sys_path = ten_py_get_path();
    TEN_LOGI("[Python addon loader] python initialized, sys.path: %s",
             sys_path);

    ten_py_mem_free((void *)sys_path);

    start_debugpy_server_if_needed();

    if (load_all_on_init) {
      // Traverse `ten_packages/extension` directory and import module.
      load_python_extensions_according_to_app_manifest_dependencies();
    } else {
      TEN_LOGI(
          "[Python addon loader] load_all_on_init is false, skip loading all "
          "python extensions when startup.");
    }

    // The `app_base_dir` is no longer needed afterwards, so it is released.
    ten_string_destroy(app_base_dir);
    app_base_dir = nullptr;

    py_thread_state_ = ten_py_eval_save_thread();

    ten_env.on_init_done();
  }

  void on_deinit(ten::ten_env_t &ten_env) override {
    // Do some de-initializations.
    if (py_thread_state_ != nullptr) {
      ten_py_eval_restore_thread(py_thread_state_);
    }

    if (py_init_by_self_) {
      int rc = ten_py_finalize();
      if (rc < 0) {
        TEN_LOGF(
            "[Python addon loader] Failed to finalize python runtime, rc: %d",
            rc);

        TEN_ASSERT(0, "Should not happen.");
      }
    }

    ten_env.on_deinit_done();
  }

  // **Note:** This function, used to dynamically load other addons, may be
  // called from multiple threads. Therefore, it must be thread-safe. Since it
  // calls `ten_py_gil_state_ensure` and `ten_py_gil_state_release`, thread
  // safety is ensured.
  void on_load_addon(TEN_UNUSED ten::ten_env_t &ten_env,
                     TEN_ADDON_TYPE addon_type, const char *addon_name,
                     void *context) override {
    // Load the specified addon.
    TEN_LOGD("[Python addon loader] on_load_addon, %s:%s",
             ten_addon_type_to_string(addon_type), addon_name);

    void *ten_py_gil_state = ten_py_gil_state_ensure();

    // Construct the full module name.
    ten_string_t *full_module_name = ten_string_create_formatted(
        "ten_packages.%s.%s", ten_addon_type_to_string(addon_type), addon_name);

    TEN_LOGD("[Python addon loader] acquired GIL, full_module_name: %s",
             ten_string_get_raw_str(full_module_name));

    // Import the specified Python module.
    bool import_status =
        ten_py_import_module(ten_string_get_raw_str(full_module_name));
    if (!import_status) {
      TEN_LOGD("[Python addon loader] Failed to import module %s",
               ten_string_get_raw_str(full_module_name));
    }

    ten_string_destroy(full_module_name);

    ten_py_gil_state_release(ten_py_gil_state);

    TEN_LOGD("[Python addon loader] released GIL");

    ten::ten_env_internal_accessor_t::on_load_addon_done(ten_env, context);
  }

 private:
  void *py_thread_state_ = nullptr;
  bool py_init_by_self_ = false;
  bool load_all_on_init = false;
  ten_string_t *app_base_dir = nullptr;

  void find_app_base_dir() {
    ten_string_t *module_path =
        ten_path_get_module_path(reinterpret_cast<const void *>(foo));
    TEN_ASSERT(module_path, "Failed to get module path.");

    app_base_dir = ten_find_base_dir(ten_string_get_raw_str(module_path),
                                     TEN_STR_APP, nullptr);
    ten_string_destroy(module_path);
  }

  // Setup python system path and make sure following paths are included:
  // <app_root>/ten_packages/system/ten_runtime_python/lib
  // <app_root>/ten_packages/system/ten_runtime_python/interface
  // <app_root>
  //
  // The reason for adding `<app_root>` to `sys.path` is that when using
  // `PyImport_Import` to load Python packages under `ten_packages/`, the module
  // name used will be in the form of `ten_packages.extensions.xxx`. Therefore,
  // `<app_root>` must be in `sys.path` to ensure that `ten_packages` can be
  // located.
  void complete_sys_path() {
    ten_list_t paths;
    ten_list_init(&paths);

    ten_string_t *lib_path = ten_string_create_formatted(
        "%s/ten_packages/system/ten_runtime_python/lib",
        ten_string_get_raw_str(app_base_dir));
    ten_string_t *interface_path = ten_string_create_formatted(
        "%s/ten_packages/system/ten_runtime_python/interface",
        ten_string_get_raw_str(app_base_dir));

    ten_list_push_str_back(&paths, ten_string_get_raw_str(lib_path));
    ten_list_push_str_back(&paths, ten_string_get_raw_str(interface_path));
    ten_list_push_str_back(&paths, ten_string_get_raw_str(app_base_dir));

    ten_string_destroy(lib_path);
    ten_string_destroy(interface_path);

    ten_py_add_paths_to_sys(&paths);

    ten_list_clear(&paths);
  }

  // Get the real path of <app_root>/ten_packages/extension/
  ten_string_t *get_addon_extensions_path() {
    ten_string_t *result = ten_string_clone(app_base_dir);
    ten_string_append_formatted(result, "/ten_packages/extension/");
    return result;
  }

  void load_python_extensions_according_to_app_manifest_dependencies() {
    ten_string_t *addon_extensions_path = get_addon_extensions_path();

    load_all_python_modules(addon_extensions_path);

    register_all_addons();

    ten_string_destroy(addon_extensions_path);
  }

  // Start the debugpy server according to the environment variable and wait for
  // the debugger to connect.
  static void start_debugpy_server_if_needed() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *enable_python_debug = getenv("TEN_ENABLE_PYTHON_DEBUG");
    if (enable_python_debug == nullptr ||
        strcmp(enable_python_debug, "true") != 0) {
      return;
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *python_debug_host = getenv("TEN_PYTHON_DEBUG_HOST");
    if (python_debug_host == nullptr) {
      python_debug_host = "localhost";
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char *python_debug_port = getenv("TEN_PYTHON_DEBUG_PORT");
    if (python_debug_port == nullptr) {
      python_debug_port = "5678";
    }

    // Make sure the port is valid.
    char *endptr = nullptr;
    int64_t port = std::strtol(python_debug_port, &endptr, 10);
    if (*endptr != '\0' || port <= 0 || port > 65535) {
      TEN_LOGE("[Python addon loader] Invalid python debug port: %s",
               python_debug_port);
      return;
    }

    ten_string_t *start_debug_server_script = ten_string_create_formatted(
        "import debugpy\n"
        "debugpy.listen(('%s', %d))\n"
        "debugpy.wait_for_client()\n",
        python_debug_host, port);

    ten_py_run_simple_string(ten_string_get_raw_str(start_debug_server_script));

    ten_string_destroy(start_debug_server_script);

    TEN_LOGI("[Python addon loader] Python debug server started at %s:%" PRId64,
             python_debug_host, port);
  }

  // Load all python addons by import modules.
  static void load_all_python_modules(ten_string_t *addon_extensions_path) {
    if (addon_extensions_path == nullptr ||
        ten_string_is_empty(addon_extensions_path)) {
      TEN_LOGE(
          "[Python addon loader] Failed to load python modules due to empty "
          "addon extension path.");
      return;
    }

    ten_dir_fd_t *dir =
        ten_path_open_dir(ten_string_get_raw_str(addon_extensions_path));
    if (dir == nullptr) {
      TEN_LOGE(
          "[Python addon loader] Failed to open directory %s when loading "
          "python modules.",
          ten_string_get_raw_str(addon_extensions_path));
      return;
    }

    ten_path_itor_t *itor = ten_path_get_first(dir);
    while (itor != nullptr) {
      ten_string_t *short_name = ten_path_itor_get_name(itor);
      if (short_name == nullptr) {
        TEN_LOGE("[Python addon loader] Failed to get short name under path %s",
                 ten_string_get_raw_str(addon_extensions_path));
        itor = ten_path_get_next(itor);
        continue;
      }

      if (!(ten_string_is_equal_c_str(short_name, ".") ||
            ten_string_is_equal_c_str(short_name, ".."))) {
        // The full module name is "ten_packages.extension.<short_name>"
        ten_string_t *full_module_name = ten_string_create_formatted(
            "ten_packages.extension.%s", ten_string_get_raw_str(short_name));
        ten_py_import_module(ten_string_get_raw_str(full_module_name));
        ten_string_destroy(full_module_name);
      }

      ten_string_destroy(short_name);
      itor = ten_path_get_next(itor);
    }

    if (dir != nullptr) {
      ten_path_close_dir(dir);
    }
  }

  static void register_all_addons() {
    ten_py_run_simple_string(
        "from ten_runtime import _AddonManager\n"
        "_AddonManager.register_all_addons(None)\n");
  }

  static void load_python_lib() {
    ten_string_t *python_lib_path =
        ten_string_create_formatted("libten_runtime_python.so");

    // The libten_runtime_python.so must be loaded globally using dlopen, and
    // cannot be a regular shared library dependency. Note that the 2nd
    // parameter must be 0 (as_local = false).
    //
    // Refer to
    // https://mail.python.org/pipermail/new-bugs-announce/2008-November/003322.html
    ten_module_load(python_lib_path, 0);

    ten_string_destroy(python_lib_path);
  }
};

TEN_CPP_REGISTER_ADDON_AS_ADDON_LOADER(python_addon_loader,
                                       python_addon_loader_t);

}  // namespace
