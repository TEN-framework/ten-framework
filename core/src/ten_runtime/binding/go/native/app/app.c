//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "ten_runtime/binding/go/interface/ten_runtime/app.h"

#include "include_internal/ten_runtime/app/app.h"
#include "include_internal/ten_runtime/binding/go/app/app.h"
#include "include_internal/ten_runtime/binding/go/internal/common.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env.h"
#include "include_internal/ten_runtime/binding/go/ten_env/ten_env_internal.h"
#include "include_internal/ten_runtime/global/signal.h"
#include "ten_runtime/binding/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/common.h"
#include "ten_runtime/binding/go/interface/ten_runtime/ten_env.h"
#include "ten_runtime/ten.h"
#include "ten_runtime/ten_env_proxy/ten_env_proxy.h"
#include "ten_utils/lib/alloc.h"

void tenGoAppOnConfigure(ten_go_handle_t go_app, ten_go_handle_t go_ten_env);

void tenGoAppOnInit(ten_go_handle_t go_app, ten_go_handle_t go_ten_env);

void tenGoAppOnDeinit(ten_go_handle_t go_app, ten_go_handle_t go_ten_env);

bool ten_go_app_check_integrity(ten_go_app_t *self) {
  TEN_ASSERT(self, "Should not happen.");

  if (ten_signature_get(&self->signature) != TEN_GO_APP_SIGNATURE) {
    return false;
  }

  return true;
}

static void proxy_on_configure(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_app_get_ten_env(app) == ten_env, "Should not happen.");

  ten_go_app_t *app_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(app_bridge, "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);
  ten_env_bridge->c_ten_env_proxy = ten_env_proxy_create(ten_env, 1, NULL);

  tenGoAppOnConfigure(app_bridge->bridge.go_instance,
                      ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_init(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");
  TEN_ASSERT(ten_env, "Should not happen.");
  TEN_ASSERT(ten_env_check_integrity(ten_env, true), "Should not happen.");
  TEN_ASSERT(ten_app_get_ten_env(app) == ten_env, "Should not happen.");

  ten_go_app_t *app_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(app_bridge, "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoAppOnInit(app_bridge->bridge.go_instance,
                 ten_go_ten_env_go_handle(ten_env_bridge));
}

static void proxy_on_deinit(ten_app_t *app, ten_env_t *ten_env) {
  TEN_ASSERT(app, "Should not happen.");
  TEN_ASSERT(ten_app_check_integrity(app, true), "Should not happen.");

  ten_go_app_t *app_bridge =
      ten_binding_handle_get_me_in_target_lang((ten_binding_handle_t *)app);
  TEN_ASSERT(app_bridge, "Should not happen.");

  ten_go_ten_env_t *ten_env_bridge = ten_go_ten_env_wrap(ten_env);

  tenGoAppOnDeinit(app_bridge->bridge.go_instance,
                   ten_go_ten_env_go_handle(ten_env_bridge));
}

static void ten_go_app_destroy(ten_go_app_t *self) {
  TEN_ASSERT(self && ten_go_app_check_integrity(self), "Should not happen.");

  ten_app_destroy(self->c_app);
  TEN_FREE(self);
}

ten_go_app_t *ten_go_app_create(ten_go_handle_t go_app_index) {
  ten_go_app_t *app_bridge = (ten_go_app_t *)TEN_MALLOC(sizeof(ten_go_app_t));
  TEN_ASSERT(app_bridge, "Failed to allocate memory.");

  ten_signature_set(&app_bridge->signature, TEN_GO_APP_SIGNATURE);
  app_bridge->bridge.go_instance = go_app_index;

  // The app bridge instance is created and managed only by Go. When the Go app
  // is finalized, the app bridge instance will be destroyed. Therefore, the C
  // part should not hold any reference to the app bridge instance.
  app_bridge->bridge.sp_ref_by_go =
      ten_shared_ptr_create(app_bridge, ten_go_app_destroy);
  app_bridge->bridge.sp_ref_by_c = NULL;

  app_bridge->c_app =
      ten_app_create(proxy_on_configure, proxy_on_init, proxy_on_deinit, NULL);
  ten_binding_handle_set_me_in_target_lang(
      (ten_binding_handle_t *)(app_bridge->c_app), app_bridge);

  // Setup the default signal handler for GO app. The reason for setting up the
  // signal handler for GO app is as follows.
  //
  // 1. Because of the linked mechanism, the following function
  // `ten_global_setup_signal_stuff()` will be called after the GO process is
  // created, and before the GO runtime is initialized. Refer to
  // `TEN_CONSTRUCTOR`.
  //
  // 2. Then the GO runtime starts, and a default signal handler is set up in
  // the GO world. The `sigaction` function is called from GO using cgo, and the
  // handler setup by `ten_global_setup_signal_stuff()` in the above step is
  // replaced.
  //
  // The code snippet of handling the `SIGINT` and `SIGTERM` in GO runtime is as
  // follows.
  //
  //   /* Setup the default signal handler in GO.*/
  //   func setsig(i uint32, fn uintptr) {
  //     var sa sigactiont
  //     sa.sa_flags = _SA_SIGINFO | _SA_ONSTACK | _SA_RESTORER | _SA_RESTART
  //     if GOARCH == "386" || GOARCH == "amd64" {
  //       sa.sa_restorer = abi.FuncPCABI0(sigreturn__sigaction)
  //     }
  //
  //     sigaction(i, &sa, nil)
  //   }
  //
  //   /* The default signal handler in GO. */
  //   func sigfwdgo(sig uint32, ...) bool {
  //     /* We are not handling the signal and there is no other handler to */
  //     /* forward to. Crash with the default behavior. */
  // 		 if fwdFn == _SIG_DFL {
  // 			 setsig(sig, _SIG_DFL)
  // 			 dieFromSignal(sig)
  // 			 return false
  // 		 }
  //
  // 		 sigfwd(fwdFn, sig, info, ctx)
  // 		 return true
  //   }
  //
  // In brief, the GO runtime will setup a default signal handler, and save the
  // old handler (i.e., ten_global_signal_handler). When GO runtime receives a
  // `SIGINT` or `SIGTERM`, it forwards the signal to the old handler first, and
  // then crashes the process as SIGKILL.
  //
  // However, the signal handler (i.e., ten_global_signal_handler) is an async
  // function, which means after the handler returns, the TEN app may not be
  // closed completely yet, and the `on_stop` or `on_deinit` callback of
  // extensions may not be called.
  //
  // 3. After the GO runtime starts, the GO `main` function will be called. Any
  // `sigaction` function called in the `main` function will replace the default
  // signal handler setup by the GO runtime. Ex: the following function
  // ten_global_setup_signal_stuff().

  ten_global_setup_signal_stuff();

  return app_bridge;
}

void ten_go_app_run(ten_go_app_t *app_bridge, bool run_in_background) {
  TEN_ASSERT(app_bridge && ten_go_app_check_integrity(app_bridge),
             "Should not happen.");

  ten_app_run(app_bridge->c_app, run_in_background, NULL);
}

void ten_go_app_close(ten_go_app_t *app_bridge) {
  TEN_ASSERT(app_bridge && ten_go_app_check_integrity(app_bridge),
             "Should not happen.");

  ten_app_close(app_bridge->c_app, NULL);
}

void ten_go_app_wait(ten_go_app_t *app_bridge) {
  TEN_ASSERT(app_bridge && ten_go_app_check_integrity(app_bridge),
             "Should not happen.");

  ten_app_wait(app_bridge->c_app, NULL);
}

void ten_go_app_finalize(ten_go_app_t *self) {
  // The Go app is finalized, it's time to destroy the C app.
  TEN_ASSERT(self && ten_go_app_check_integrity(self), "Should not happen.");

  ten_go_bridge_destroy_go_part(&self->bridge);
}
