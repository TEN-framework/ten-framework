//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include <uv.h>

#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "node.h"
#include "v8-script.h"
// Header file region splitter.
#include "include_internal/ten_runtime/addon/addon_host.h"
#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/cpp/detail/addon_loader.h"
#include "include_internal/ten_runtime/binding/cpp/detail/addon_manager.h"
#include "include_internal/ten_runtime/binding/cpp/ten.h"
#include "ten_utils/lib/module.h"

using node::CommonEnvironmentSetup;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Locker;
using v8::MaybeLocal;
using v8::V8;
using v8::Value;

using notify_func_t = std::function<void(ten::ten_env_t &)>;

static void foo() {}

namespace {

class nodejs_addon_loader_t;

class load_addon_data_t {
 public:
  std::string addon_name;
  nodejs_addon_loader_t *loader;
  void *context;

  load_addon_data_t(const char *addon_name, nodejs_addon_loader_t *loader,
                    void *context)
      : addon_name(addon_name), loader(loader), context(context) {}
};

class nodejs_addon_loader_t : public ten::addon_loader_t {
 public:
  explicit nodejs_addon_loader_t(const char *name) { (void)name; };

  ~nodejs_addon_loader_t() override {
    // Wait for the Node.js thread to terminate to prevent abnormal program
    // exit.
    if (this->node_thread_.joinable()) {
      this->node_thread_.join();
    }
  }

  // @{
  nodejs_addon_loader_t(nodejs_addon_loader_t &other) = delete;
  nodejs_addon_loader_t(nodejs_addon_loader_t &&other) = delete;
  nodejs_addon_loader_t &operator=(const nodejs_addon_loader_t &other) = delete;
  nodejs_addon_loader_t &operator=(nodejs_addon_loader_t &&other) = delete;
  // @}

  void on_init(ten::ten_env_t &ten_env) override {
    this->ten_env_ = &ten_env;

    // Do some initializations.
    load_nodejs_lib();

    try {
      std::vector<std::string> args;

      args.emplace_back("node");

      // Allow manual invocation of `global.gc()` to trigger garbage collection.
      args.emplace_back("--expose-gc");
      // Enable Node.js warning tracking for easier debugging.
      args.emplace_back("--trace-warnings");

      args.emplace_back("-e");
      args.emplace_back("console.log('foo');");

      // Globally initialize the Node.js runtime to ensure that Node.js can run
      // properly in the current process.
      std::shared_ptr<node::InitializationResult> result =
          node::InitializeOncePerProcess(
              args,
              {
                  // Do not automatically initialize the V8 engine, as it needs
                  // to be initialized in a new thread.
                  node::ProcessInitializationFlags::kNoInitializeV8,

                  // Do not automatically initialize the Node.js V8 platform; it
                  // will be initialized manually later.
                  node::ProcessInitializationFlags::kNoInitializeNodeV8Platform,

                  // Disable the `NODE_OPTIONS` environment variable to prevent
                  // external environments from affecting the execution of
                  // Node.js.
                  //
                  // This is used to test NODE_REPL_EXTERNAL_MODULE is disabled
                  // with kDisableNodeOptionsEnv. If other tests need
                  // NODE_OPTIONS support in the future, split this
                  // configuration out as a command line option.
                  node::ProcessInitializationFlags::kDisableNodeOptionsEnv,
              });

      for (const std::string &error : result->errors()) {
        std::cerr << "Nodejs addon loader error: " << error << '\n';
      }

      if (result->early_return()) {
        std::cerr << "Nodejs addon loader early return: " << result->exit_code()
                  << '\n';
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        exit(result->exit_code());
      }

      // RunNodeInstance in another thread.
      this->node_thread_ = std::thread([this, args]() {
        // MultiIsolatePlatform : Allow multiple `v8::Isolate` instances to run
        // on the same platform.
        std::unique_ptr<MultiIsolatePlatform> platform =
            MultiIsolatePlatform::Create(4);

        // Initialize the V8 platform to enable multi-threaded execution.
        V8::InitializePlatform(platform.get());

        // Start the V8 engine to enable the execution of JavaScript code.
        V8::Initialize();

        // Start the Node.js event loop.
        int ret = RunNodeWithESM(platform.get(), args, {});

        // Wait for the Node.js event loop to terminate.

        std::cout << "Node Instance run completed with code: " << ret << '\n';

        // Release V8 and Node.js resources.
        V8::Dispose();
        V8::DisposePlatform();
        node::TearDownOncePerProcess();
      });
    } catch (const std::exception &e) {
      std::cerr << "Nodejs addon loader init exception: " << e.what() << '\n';
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(1);
    }
  }

  // Clean up and shut down the Node.js addon loader to ensure resource
  // release, garbage collection execution, and proper termination of the
  // Node.js thread.
  void on_deinit(ten::ten_env_t &ten_env) override {
    if (this->setup_ == nullptr || this->event_loop_ == nullptr) {
      ten_env.on_deinit_done();
      return;
    }

    // Create a `uv_async_t` event to perform asynchronous operations within
    // the libuv event loop.
    auto *deinit_handle = new uv_async_t();

    deinit_handle->data = this;

    uv_async_init(this->event_loop_, deinit_handle, [](uv_async_t *handle) {
      auto *this_ptr = static_cast<nodejs_addon_loader_t *>(handle->data);

      Locker locker(this_ptr->setup_->isolate());
      Isolate::Scope isolate_scope(this_ptr->setup_->isolate());
      HandleScope handle_scope(this_ptr->setup_->isolate());
      Context::Scope context_scope(this_ptr->setup_->context());

      std::string js_code =
          "global.ten_runtime_nodejs.AddonManager.getInstance().deinit();"
          "global.gc();"
          "console.log('gc done');";

      // Convert the JavaScript code into a `v8::String`.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8(this_ptr->setup_->isolate(), js_code.c_str(),
                                  v8::NewStringType::kNormal)
              .ToLocalChecked();

      // Compile the JS codes.
      v8::Local<v8::Script> script =
          v8::Script::Compile(this_ptr->setup_->context(), source)
              .ToLocalChecked();

      // Run the JS codes.
      script->Run(this_ptr->setup_->context()).ToLocalChecked();

      // Close the `uv_async_t` event to ensure that the libuv event loop no
      // longer executes it.
      uv_close(reinterpret_cast<uv_handle_t *>(handle),
               [](uv_handle_t *handle) {
                 auto *async_handle = reinterpret_cast<uv_async_t *>(handle);
                 delete async_handle;
               });

      // Stop the Node.js runtime environment to ensure that all resources are
      // properly released.
      node::Stop(this_ptr->setup_->env());

      this_ptr->notify(
          [](ten::ten_env_t &ten_env) { ten_env.on_deinit_done(); });
    });
    uv_async_send(deinit_handle);
  }

  // Note: This function, used to dynamically load other addons, may be called
  // from multiple threads. Therefore, it must be thread-safe.
  //
  // It uses the libuv event loop (`uv_async_t`) to execute JavaScript code
  // within the Node.js thread, allowing for the loading and registration of
  // TEN addons.
  void on_load_addon(TEN_UNUSED ten::ten_env_t &ten_env,
                     TEN_UNUSED TEN_ADDON_TYPE addon_type,
                     const char *addon_name, void *context) override {
    if (this->setup_ == nullptr || this->event_loop_ == nullptr) {
      std::cerr << "Nodejs addon loader not initialized" << '\n';
      ten::ten_env_internal_accessor_t::on_load_addon_done(ten_env, context);
      return;
    }

    load_addon_with_esm(addon_name, context);
  }

 private:
  struct proxy_notify_ctx_t {
    notify_func_t func;
    ten::ten_env_t *ten_env;
  };

  static void proxy_notify(void *from, void *args) {
    auto *app = static_cast<ten_app_t *>(from);
    TEN_ASSERT(app && ten_app_check_integrity(app, true), "Should not happen.");

    auto *proxy_notify_ctx = static_cast<proxy_notify_ctx_t *>(args);
    TEN_ASSERT(proxy_notify_ctx, "Should not happen.");

    ten::ten_env_t *ten_env = proxy_notify_ctx->ten_env;
    TEN_ASSERT(ten_env, "Should not happen.");

    proxy_notify_ctx->func(*ten_env);

    delete proxy_notify_ctx;
  }

  void notify(const notify_func_t &func) {
    auto *addon_loader_c_instance =
        static_cast<ten_addon_loader_t *>(get_c_instance());
    TEN_ASSERT(
        addon_loader_c_instance && ten_addon_loader_check_integrity(
                                       addon_loader_c_instance,
                                       // TEN_NOLINTNEXTLINE(thread-check)
                                       // thread-check: This function is
                                       // intended to be called in any threads.
                                       false),
        "Should not happen.");

    ten_addon_host_t *addon_host = addon_loader_c_instance->addon_host;
    TEN_ASSERT(addon_host, "Should not happen.");

    ten_app_t *app = addon_host->attached_app;
    TEN_ASSERT(
        app && ten_app_check_integrity(app,
                                       // TEN_NOLINTNEXTLINE(thread-check)
                                       // thread-check: This function is
                                       // intended to be called in any threads.
                                       false),
        "Should not happen.");

    auto *proxy_notify_ctx = new proxy_notify_ctx_t{func, this->ten_env_};

    int rc = ten_runloop_post_task_tail(ten_app_get_attached_runloop(app),
                                        proxy_notify, app, proxy_notify_ctx);
    TEN_ASSERT(!rc, "Should not happen.");
  }

  // Set up the Node.js instance for execution, and start the Node.js event
  // loop, and run code inside of it.
  //
  // This function is used to run the Node.js instance and load the ESM module.
  int RunNodeWithESM(MultiIsolatePlatform *platform,
                     const std::vector<std::string> &args,
                     const std::vector<std::string> &exec_args) {
    int exit_code = 0;

    std::vector<std::string> errors;

    // Setup libuv event loop, v8::Isolate, and Node.js Environment.
    std::unique_ptr<CommonEnvironmentSetup> setup =
        CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);
    if (!setup) {
      for (const std::string &err : errors) {
        std::cerr << "Error: " << err << '\n';
      }

      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(1);
    }

    // Get references to these resources.
    this->setup_ = std::move(setup);
    this->event_loop_ = this->setup_->event_loop();
    auto *isolate = this->setup_->isolate();
    auto *env = this->setup_->env();

    {
      // Ensure that the current thread holds the lock of the v8::Isolate to
      // guarantee thread safety.
      Locker locker(isolate);

      // Enter the current v8::Isolate's scope, ensuring that all V8 operations
      // are performed within a specific v8::Isolate.
      Isolate::Scope isolate_scope(isolate);

      // Create a handle scope to manage the lifecycle of JavaScript objects and
      // prevent memory leaks.
      HandleScope handle_scope(isolate);

      // Enter the JavaScript execution context (Context) to ensure that
      // JavaScript code can run properly.
      //
      // The v8::Context needs to be entered when node::CreateEnvironment() and
      // node::LoadEnvironment() are being called.
      Context::Scope context_scope(this->setup_->context());

      // Define the callback function.
      v8::Local<v8::Context> context = this->setup_->context();
      v8::Local<v8::Object> global = context->Global();

      // Define the callback function which will be called when the import
      // is completed.
      v8::Local<v8::Function> callback_fn =
          v8::Function::New(
              context,
              [](const v8::FunctionCallbackInfo<v8::Value> &args) {
                v8::Local<v8::External> data =
                    v8::Local<v8::External>::Cast(args.Data());
                auto *this_ptr =
                    static_cast<nodejs_addon_loader_t *>(data->Value());

                this_ptr->notify(
                    [](ten::ten_env_t &ten_env) { ten_env.on_init_done(); });
              },
              v8::External::New(context->GetIsolate(), this))
              .ToLocalChecked();

      // Set the callback function as a global property.
      global
          ->Set(context,
                v8::String::NewFromUtf8(context->GetIsolate(),
                                        "__ten_runtime_nodejs_module_imported",
                                        v8::NewStringType::kNormal)
                    .ToLocalChecked(),
                callback_fn)
          .Check();

      // Run JavaScript code within the Isolate
      //
      // `setInterval(() => {}, 1000);` creates an empty `setInterval` task to
      // ensure that the event loop does not exit, allowing the Node.js instance
      // to keep running continuously.
      MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
          env,
          "(async () => { "
#if defined(_DEBUG)
          "  console.log('wait 3 seconds to mock import slowly...');"
          "  await new Promise(resolve => setTimeout(resolve, 3000));"
#endif
          "  const module = await "
          "import(process.cwd() + "
          "'/ten_packages/system/ten_runtime_nodejs/build/index.js');"
          "  global.ten_runtime_nodejs = module;"
          "  console.log('ten_runtime_nodejs module loaded successfully');"
          "  global.__ten_runtime_nodejs_module_imported();"
          "})();"
          "setInterval(() => {}, 1000);");

      if (loadenv_ret.IsEmpty()) {
        std::cerr << "Nodejs addon loader throw an exception during loading "
                     "the ten_runtime_nodejs module"
                  << '\n';
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        exit(1);
      }

      // Start the Node.js event loop.
      exit_code = node::SpinEventLoop(env).FromMaybe(1);
    }

    this->setup_.reset();

    return exit_code;
  }

  // Run the Node.js instance and load the CJS module.
  int RunNodeWithCJS(MultiIsolatePlatform *platform,
                     const std::vector<std::string> &args,
                     const std::vector<std::string> &exec_args) {
    int exit_code = 0;

    std::vector<std::string> errors;

    // Setup libuv event loop, v8::Isolate, and Node.js Environment.
    std::unique_ptr<CommonEnvironmentSetup> setup =
        CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);
    if (!setup) {
      for (const std::string &err : errors) {
        std::cerr << "Error: " << err << '\n';
      }

      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(1);
    }

    // Get references to these resources.
    this->setup_ = std::move(setup);
    this->event_loop_ = this->setup_->event_loop();
    auto *isolate = this->setup_->isolate();
    auto *env = this->setup_->env();

    {
      // Ensure that the current thread holds the lock of the v8::Isolate to
      // guarantee thread safety.
      Locker locker(isolate);

      // Enter the current v8::Isolate's scope, ensuring that all V8 operations
      // are performed within a specific v8::Isolate.
      Isolate::Scope isolate_scope(isolate);

      // Create a handle scope to manage the lifecycle of JavaScript objects and
      // prevent memory leaks.
      HandleScope handle_scope(isolate);

      // Enter the JavaScript execution context (Context) to ensure that
      // JavaScript code can run properly.
      //
      // The v8::Context needs to be entered when node::CreateEnvironment() and
      // node::LoadEnvironment() are being called.
      Context::Scope context_scope(this->setup_->context());

      // Run JavaScript code within the Isolate and load the
      // `ten_runtime_nodejs` module.
      //
      // There is also a variant that takes a callback and provides it with
      // the `require` and `process` objects, so that it can manually compile
      // and run scripts as needed.
      //
      // The `require` function inside this script does *not* access the file
      // system, and can only load built-in Node.js modules.
      //
      // `module.createRequire()` is being used to create one that is able to
      // load files from the disk, and uses the standard CommonJS file loader
      // instead of the internal-only `require` function.
      //
      // `setInterval(() => {}, 1000);` creates an empty `setInterval` task to
      // ensure that the event loop does not exit, allowing the Node.js instance
      // to keep running continuously.
      MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
          env,
          "js_require = require('module').createRequire(process.cwd() + "
          "'/');"
          "global.ten_runtime_nodejs = "
          "js_require('./ten_packages/system/ten_runtime_nodejs');"
          "setInterval(() => {}, 1000);");

      if (loadenv_ret.IsEmpty()) {
        // There has been a JS exception.
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        exit(1);
      }

      notify([](ten::ten_env_t &ten_env) { ten_env.on_init_done(); });

      // Start the Node.js event loop.
      exit_code = node::SpinEventLoop(env).FromMaybe(1);
    }

    this->setup_.reset();

    return exit_code;
  }

  // Dynamically load the `libnode.so` shared library to ensure the Node.js
  // runtime environment is available.
  static void load_nodejs_lib() {
    ten_string_t *module_path =
        ten_path_get_module_path(reinterpret_cast<const void *>(foo));
    TEN_ASSERT(module_path, "Failed to get module path.");

    ten_string_t *nodejs_lib_path = ten_string_create_formatted(
        "%s/libnode.so", ten_string_get_raw_str(module_path));

    // The `libnode.so` must be loaded globally using `dlopen` to ensure that
    // all Node.js components can access it. Note that the second parameter must
    // be `0` (`as_local = false`).
    ten_module_load(nodejs_lib_path, 0);

    ten_string_destroy(nodejs_lib_path);
    ten_string_destroy(module_path);
  }

  void load_addon_with_cjs(const char *addon_name, void *context) {
    auto *load_addon_handle = new uv_async_t();

    auto *load_addon_data = new load_addon_data_t(addon_name, this, context);
    load_addon_handle->data = load_addon_data;

    uv_async_init(this->event_loop_, load_addon_handle, [](uv_async_t *handle) {
      auto *load_addon_data = static_cast<load_addon_data_t *>(handle->data);

      auto *this_ptr = load_addon_data->loader;
      std::string addon_name = load_addon_data->addon_name;

      // Ensures thread safety.
      Locker locker(this_ptr->setup_->isolate());

      // Enters the V8 Isolate environment.
      Isolate::Scope isolate_scope(this_ptr->setup_->isolate());

      // Creates a V8 handle scope to prevent memory leaks.
      HandleScope handle_scope(this_ptr->setup_->isolate());

      // Enters the JavaScript execution context, allowing the addon
      // loading code to run correctly.
      Context::Scope context_scope(this_ptr->setup_->context());

      std::string js_code =
          "global.ten_runtime_nodejs.AddonManager.getInstance()."
          "loadSingleAddon("
          "'" +
          addon_name + "');";

      // Convert the JavaScript code into a `v8::String`.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8(this_ptr->setup_->isolate(), js_code.c_str(),
                                  v8::NewStringType::kNormal)
              .ToLocalChecked();

      // Compile the JS codes.
      v8::Local<v8::Script> script =
          v8::Script::Compile(this_ptr->setup_->context(), source)
              .ToLocalChecked();

      // Run the JS codes.
      script->Run(this_ptr->setup_->context()).ToLocalChecked();

      // Close the `uv_async_t` event to ensure that the libuv event loop
      // no longer executes it.
      uv_close(reinterpret_cast<uv_handle_t *>(handle),
               [](uv_handle_t *handle) {
                 auto *async_handle = reinterpret_cast<uv_async_t *>(handle);
                 delete async_handle;
               });

      this_ptr->notify([load_addon_data](ten::ten_env_t &ten_env) {
        ten::ten_env_internal_accessor_t::on_load_addon_done(
            ten_env, load_addon_data->context);
      });
    });

    uv_async_send(load_addon_handle);
  }

  void load_addon_with_esm(const char *addon_name, void *context) {
    auto *load_addon_handle = new uv_async_t();

    auto *load_addon_data = new load_addon_data_t(addon_name, this, context);
    load_addon_handle->data = load_addon_data;

    uv_async_init(this->event_loop_, load_addon_handle, [](uv_async_t *handle) {
      auto *load_addon_data = static_cast<load_addon_data_t *>(handle->data);

      auto *this_ptr = load_addon_data->loader;
      std::string addon_name = load_addon_data->addon_name;

      // Ensures thread safety.
      Locker locker(this_ptr->setup_->isolate());

      // Enters the V8 Isolate environment.
      Isolate::Scope isolate_scope(this_ptr->setup_->isolate());

      // Creates a V8 handle scope to prevent memory leaks.
      HandleScope handle_scope(this_ptr->setup_->isolate());

      // Enters the JavaScript execution context, allowing the addon
      // loading code to run correctly.
      Context::Scope context_scope(this_ptr->setup_->context());

      // Define the callback function.
      v8::Local<v8::Context> context = this_ptr->setup_->context();
      v8::Local<v8::Object> global = context->Global();

      // Define the callback function which will be called when the import
      // is completed.
      v8::Local<v8::Function> callback_fn =
          v8::Function::New(
              context,
              [](const v8::FunctionCallbackInfo<v8::Value> &args) {
                v8::Local<v8::External> data =
                    v8::Local<v8::External>::Cast(args.Data());
                auto *load_addon_data =
                    static_cast<load_addon_data_t *>(data->Value());

                load_addon_data->loader->notify(
                    [load_addon_data](ten::ten_env_t &ten_env) {
                      ten::ten_env_internal_accessor_t::on_load_addon_done(
                          ten_env, load_addon_data->context);
                      delete load_addon_data;
                    });
              },
              v8::External::New(context->GetIsolate(), load_addon_data))
              .ToLocalChecked();

      // Set the callback function as a global property.
      std::string callback_name =
          "__registerAddonCompletedCallback_" + addon_name + "_" +
          std::to_string(reinterpret_cast<uintptr_t>(load_addon_data->context));

      global
          ->Set(context,
                v8::String::NewFromUtf8(context->GetIsolate(),
                                        callback_name.c_str(),
                                        v8::NewStringType::kNormal)
                    .ToLocalChecked(),
                callback_fn)
          .Check();

      // Use Promise and register callback.
      std::string js_code =
          "(() => {\n"
          "  const p = "
          "global.ten_runtime_nodejs.AddonManager.getInstance()."
          "loadSingleAddon("
          "'" +
          addon_name +
          "');\n"
          "  p.then(() => {\n"
          "    global." +
          callback_name +
          "();\n"
          "  }).catch((err) => {\n"
          "    console.error('Error registering addon:', err);\n"
          "    global." +
          callback_name +
          "();\n"
          "  });\n"
          "  return p;\n"
          "})();\n";

      // Convert and execute JavaScript code.
      v8::Local<v8::String> source =
          v8::String::NewFromUtf8(this_ptr->setup_->isolate(), js_code.c_str(),
                                  v8::NewStringType::kNormal)
              .ToLocalChecked();

      // Compile JavaScript code.
      v8::Local<v8::Script> script =
          v8::Script::Compile(this_ptr->setup_->context(), source)
              .ToLocalChecked();

      // Execute JavaScript code.
      script->Run(this_ptr->setup_->context()).ToLocalChecked();

      // Close the `uv_async_t` event to ensure that the libuv event loop
      // no longer executes it.
      uv_close(reinterpret_cast<uv_handle_t *>(handle),
               [](uv_handle_t *handle) {
                 auto *async_handle = reinterpret_cast<uv_async_t *>(handle);
                 delete async_handle;
               });
    });

    uv_async_send(load_addon_handle);
  }

  std::unique_ptr<CommonEnvironmentSetup> setup_{nullptr};
  uv_loop_s *event_loop_{nullptr};
  std::thread node_thread_;

  ten::ten_env_t *ten_env_{nullptr};
};

TEN_CPP_REGISTER_ADDON_AS_ADDON_LOADER(nodejs_addon_loader,
                                       nodejs_addon_loader_t);

}  // namespace
