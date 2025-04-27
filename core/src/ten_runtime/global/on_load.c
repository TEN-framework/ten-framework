//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/common/log.h"
#include "include_internal/ten_runtime/extension_group/builtin/builtin_extension_group.h"
#include "include_internal/ten_runtime/global/global.h"
#include "include_internal/ten_runtime/global/signal.h"
#include "include_internal/ten_utils/backtrace/backtrace.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_utils/macro/ctor.h"
#include "ten_utils/sanitizer/memory_check.h"

// LeakSanitizer checks for memory leaks when `main` ends, but functions with
// the __attribute__((destructor)) attribute are called after LeakSanitizer
// runs. Therefore, if the result of TEN_MALLOC is placed into a global
// allocated memory queue used by ten to check for memory leaks within a
// constructor function, LeakSanitizer will mistakenly report those memory
// buffers in the global allocated memory queue as memory leaks. This happens
// because these memory buffers are freed in the destructor function, but
// LeakSanitizer performs its check before that. Therefore, we should directly
// use `ten_malloc` for malloc operations within the constructor.
//
// And memory leaks within the constructor are handled by the standard ASan
// provided by Clang/GCC.
TEN_CONSTRUCTOR(ten_runtime_on_load) {
  ten_sanitizer_memory_record_init();

#if !defined(OS_WINDOWS)
  ten_global_signal_alt_stack_create();
#endif

  ten_backtrace_create_global();  // Initialize backtrace module.
  ten_global_init();

  ten_global_setup_signal_stuff();
  ten_log_global_init();
  ten_log_global_set_output_level(DEFAULT_LOG_OUTPUT_LEVEL);
}

TEN_DESTRUCTOR(ten_runtime_on_unload) {
  ten_global_deinit();
  ten_log_global_deinit();
  ten_backtrace_destroy_global();

#if !defined(OS_WINDOWS)
  ten_global_signal_alt_stack_destroy();
#endif

  ten_sanitizer_memory_record_deinit();
}
