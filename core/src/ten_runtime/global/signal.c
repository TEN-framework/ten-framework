//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#include "include_internal/ten_runtime/global/signal.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if !defined(OS_WINDOWS)
#include <execinfo.h>
#include <pthread.h>
#include <unistd.h>
#endif

#include "include_internal/ten_runtime/app/close.h"
#include "include_internal/ten_runtime/global/global.h"
#include "include_internal/ten_utils/log/log.h"
#include "ten_runtime/app/app.h"
#include "ten_utils/container/list.h"
#include "ten_utils/container/list_node_ptr.h"
#include "ten_utils/lib/time.h"
#include "ten_utils/macro/mark.h"
#include "ten_utils/sanitizer/memory_check.h"

#if !defined(OS_WINDOWS)

// Global variable to count SIGINT signals.
static volatile sig_atomic_t sigint_count = 0;

/**
 * @brief Ignores SIGPIPE signals to prevent program termination when writing to
 * closed sockets.
 *
 * SIGPIPE is generated when a process attempts to write to a pipe or socket
 * whose reading end has been closed. By default, this signal terminates the
 * process.
 *
 * This function sets up a signal handler that ignores SIGPIPE signals, allowing
 * the write operations to fail with EPIPE error instead of terminating the
 * process. This is particularly important for network applications.
 *
 * Reference: https://github.com/joyent/libuv/issues/1254
 */
static void ten_global_ignore_sigpipe(void) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;   // Set handler to ignore the signal.
  sigemptyset(&act.sa_mask);  // Initialize signal set to empty.

  if (sigaction(SIGPIPE, &act, NULL) < 0) {
    (void)dprintf(STDERR_FILENO, "Failed to ignore SIGPIPE.\n");

    assert(0 && "Should not happen.");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Handler for SIGINT and SIGTERM signals.
 */
static void ten_global_sigint_sigterm_handler(int signo,
                                              TEN_UNUSED siginfo_t *info,
                                              TEN_UNUSED void *context) {
  (void)dprintf(STDERR_FILENO, "Received SIGINT/SIGTERM (%d)\n", signo);

  ten_mutex_lock(g_apps_mutex);

  ten_list_foreach (&g_apps, iter) {
    ten_app_t *app = ten_ptr_listnode_get(iter.node);
    assert(app && "Invalid argument.");

    ten_app_close(app, NULL);
  }

  ten_mutex_unlock(g_apps_mutex);

  sigint_count++;
  if (sigint_count >= 2) {
    (void)dprintf(STDERR_FILENO,
                  "Received SIGINT/SIGTERM (%d) twice, exit directly\n", signo);

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Handler for SIGUSR1 signal.
 */
static void ten_global_sigusr1_handler(int signo, TEN_UNUSED siginfo_t *info,
                                       TEN_UNUSED void *context) {
  (void)dprintf(STDERR_FILENO, "Received SIGUSR1 (%d)\n", signo);

  ten_sanitizer_memory_record_dump();
}

/**
 * @brief Handler for SIGSEGV signal.
 */
static void ten_global_sigsegv_handler(TEN_UNUSED int signo, siginfo_t *info,
                                       TEN_UNUSED void *context) {
  // Log crash details.
  (void)dprintf(STDERR_FILENO,
                "Segmentation fault (SIGSEGV) detected in thread 0x%lx at "
                "address: %p\n",
                (unsigned long)pthread_self(),
                info->si_addr ? info->si_addr : "(nil)");

  ten_backtrace_dump_global(0);

  // Wait for a short period to allow backtrace to be written.
  ten_sleep_ms(200);

  _exit(EXIT_FAILURE);
}

/**
 * @brief Handler for SIGHUP signal.
 */
static void ten_global_sighup_handler(int signo, TEN_UNUSED siginfo_t *info,
                                      TEN_UNUSED void *context) {
  (void)dprintf(STDERR_FILENO, "Received SIGHUP (%d)\n", signo);

  ten_log_global_reload();
}

// The alternate stack size.
#define ALT_STACK_SIZE (unsigned long)(1024 * 1024)

void *g_alt_stack = NULL;

void ten_global_signal_alt_stack_create(void) {
  assert(!g_alt_stack && "Should not happen.");

  g_alt_stack = malloc(ALT_STACK_SIZE);
  assert(g_alt_stack && "Failed to allocate alternate stack.");
}

void ten_global_signal_alt_stack_destroy(void) {
  assert(g_alt_stack && "Should not happen.");

  free(g_alt_stack);
}

/**
 * @brief Setup a specific signal handler with common configuration.
 *
 * @param signo Signal number to handle.
 * @param handler Signal handler function.
 * @param act Pointer to the sigaction structure with pre-configured flags.
 * @return 0 on success, -1 on failure.
 */
static int setup_signal_handler(int signo,
                                void (*handler)(int, siginfo_t *, void *),
                                struct sigaction *act) {
  act->sa_sigaction = handler;
  return sigaction(signo, act, NULL);
}

// Setup signal handlers for various signals.
static void ten_global_setup_sig_handler(void) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  // SA_SIGINFO flag allows the signal handler to receive additional
  // information.
  act.sa_flags = SA_SIGINFO;

  // Configure alternate stack for signal handling.
  stack_t ss;
  ss.ss_sp = g_alt_stack;
  if (ss.ss_sp == NULL) {
    (void)dprintf(STDERR_FILENO,
                  "Failed to allocate alternate stack for signal handling\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  ss.ss_size = ALT_STACK_SIZE;
  ss.ss_flags = 0;
  if (sigaltstack(&ss, NULL) == -1) {
    perror("sigaltstack");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  // SA_ONSTACK flag ensures the signal handler uses the alternate stack.
  act.sa_flags |= SA_ONSTACK;

  // Install handlers for each signal type.
  if (0 !=
      setup_signal_handler(SIGINT, ten_global_sigint_sigterm_handler, &act)) {
    (void)dprintf(STDERR_FILENO, "Failed to install SIGINT handler\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (0 !=
      setup_signal_handler(SIGTERM, ten_global_sigint_sigterm_handler, &act)) {
    (void)dprintf(STDERR_FILENO, "Failed to install SIGTERM handler\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (0 != setup_signal_handler(SIGUSR1, ten_global_sigusr1_handler, &act)) {
    (void)dprintf(STDERR_FILENO, "Failed to install SIGUSR1 handler\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (0 != setup_signal_handler(SIGSEGV, ten_global_sigsegv_handler, &act)) {
    (void)dprintf(STDERR_FILENO, "Failed to install SIGSEGV handler\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }

  if (0 != setup_signal_handler(SIGHUP, ten_global_sighup_handler, &act)) {
    (void)dprintf(STDERR_FILENO, "Failed to install SIGHUP handler\n");

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    exit(EXIT_FAILURE);
  }
}

void ten_global_setup_signal_stuff(void) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *disable_signal_trap = getenv("TEN_DISABLE_SIGNAL_TRAP");
  if (disable_signal_trap && !strcmp(disable_signal_trap, "true")) {
    // No trap signal, for nodejs / python / java bindings.
  } else {
    ten_global_ignore_sigpipe();
    ten_global_setup_sig_handler();
  }
}

#else

#include <windows.h>

static volatile LONG ctrl_c_count = 0;

/**
 * @brief Exception filter for handling access violations (segmentation faults).
 */
LONG WINAPI TenUnhandledExceptionFilter(EXCEPTION_POINTERS *ExceptionInfo) {
  // Only handle access violations (segmentation faults).
  if (ExceptionInfo->ExceptionRecord->ExceptionCode ==
      EXCEPTION_ACCESS_VIOLATION) {
    (void)fprintf(
        stderr,
        "Access violation (segmentation fault) detected in thread 0x%lx at "
        "address: 0x%p\n",
        GetCurrentThreadId(),
        (void *)ExceptionInfo->ExceptionRecord->ExceptionInformation[1]);

    (void)fprintf(stderr, "Fault type: %s memory\n",
                  ExceptionInfo->ExceptionRecord->ExceptionInformation[0]
                      ? "writing to"
                      : "reading from");

    (void)fprintf(stderr, "===================================\n");
    ten_backtrace_dump_global(0);

    // Wait for a short period to allow backtrace to be written.
    ten_sleep_ms(200);

    return EXCEPTION_EXECUTE_HANDLER;  // This will terminate the process.
  }

  return EXCEPTION_CONTINUE_SEARCH;  // Pass to the next handler.
}

BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
  switch (dwCtrlType) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
    (void)fprintf(stderr, "Received CTRL+C/CTRL+BREAK\n");

    ten_mutex_lock(g_apps_mutex);

    ten_list_foreach (&g_apps, iter) {
      ten_app_t *app = ten_ptr_listnode_get(iter.node);
      assert(app && "Invalid argument.");

      ten_app_close(app, NULL);
    }

    ten_mutex_unlock(g_apps_mutex);

    ctrl_c_count++;
    if (ctrl_c_count >= 2) {
      (void)fprintf(stderr,
                    "Received CTRL+C/CTRL+BREAK twice, exit directly\n");

      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }
    return TRUE;  // Signal has been handled.

  default:
    return FALSE;  // Signal has _not_ been handled.
  }
}

void ten_global_setup_signal_stuff(void) {
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  const char *disable_signal_trap = getenv("TEN_DISABLE_SIGNAL_TRAP");
  if (disable_signal_trap && !strcmp(disable_signal_trap, "true")) {
    // No trap signal, for nodejs / python / java bindings
  } else {
    // Register console handler for CTRL+C and CTRL+BREAK.
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
      (void)fprintf(stderr, "Failed to set control handler\n");

      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      exit(EXIT_FAILURE);
    }

    // Register exception handler for access violations (segmentation faults).
    SetUnhandledExceptionFilter(TenUnhandledExceptionFilter);
  }
}

#endif
