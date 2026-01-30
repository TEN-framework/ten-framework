/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_signal.h"

#include <cerrno>

#include "shrpx_log.h"
#include "template.h"

using namespace nghttp2;

namespace shrpx {

int shrpx_signal_block_all(sigset_t *oldset) {
  sigset_t newset;

  sigfillset(&newset);

#ifndef NOTHREADS
  int rv;

  rv = pthread_sigmask(SIG_SETMASK, &newset, oldset);

  if (rv != 0) {
    errno = rv;
    return -1;
  }

  return 0;
#else  // NOTHREADS
  return sigprocmask(SIG_SETMASK, &newset, oldset);
#endif // NOTHREADS
}

int shrpx_signal_unblock_all() {
  sigset_t newset;

  sigemptyset(&newset);

#ifndef NOTHREADS
  int rv;

  rv = pthread_sigmask(SIG_SETMASK, &newset, nullptr);

  if (rv != 0) {
    errno = rv;
    return -1;
  }

  return 0;
#else  // NOTHREADS
  return sigprocmask(SIG_SETMASK, &newset, nullptr);
#endif // NOTHREADS
}

int shrpx_signal_set(sigset_t *set) {
#ifndef NOTHREADS
  int rv;

  rv = pthread_sigmask(SIG_SETMASK, set, nullptr);

  if (rv != 0) {
    errno = rv;
    return -1;
  }

  return 0;
#else  // NOTHREADS
  return sigprocmask(SIG_SETMASK, set, nullptr);
#endif // NOTHREADS
}

namespace {
template <typename Signals>
int signal_set_handler(void (*handler)(int), Signals &&sigs) {
  struct sigaction act {};
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  int rv;
  for (auto sig : sigs) {
    rv = sigaction(sig, &act, nullptr);
    if (rv != 0) {
      return -1;
    }
  }
  return 0;
}
} // namespace

namespace {
constexpr auto main_proc_ign_signals = std::to_array({SIGPIPE});
} // namespace

namespace {
constexpr auto worker_proc_ign_signals =
  std::to_array({REOPEN_LOG_SIGNAL, EXEC_BINARY_SIGNAL,
                 GRACEFUL_SHUTDOWN_SIGNAL, RELOAD_SIGNAL, SIGPIPE});
} // namespace

int shrpx_signal_set_main_proc_ign_handler() {
  return signal_set_handler(SIG_IGN, main_proc_ign_signals);
}

int shrpx_signal_unset_main_proc_ign_handler() {
  return signal_set_handler(SIG_DFL, main_proc_ign_signals);
}

int shrpx_signal_set_worker_proc_ign_handler() {
  return signal_set_handler(SIG_IGN, worker_proc_ign_signals);
}

int shrpx_signal_unset_worker_proc_ign_handler() {
  return signal_set_handler(SIG_DFL, worker_proc_ign_signals);
}

} // namespace shrpx
