/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
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
#include "shrpx_exec.h"

#include <cerrno>

#include "shrpx_signal.h"
#include "shrpx_log.h"
#include "util.h"
#include "template.h"

using namespace nghttp2;

namespace shrpx {

// inspired by h2o_read_command function from h2o project:
// https://github.com/h2o/h2o
int exec_read_command(Process &proc, char *const argv[]) {
  int rv;
  int pfd[2];

#if defined(HAVE_PIPE2) && defined(O_CLOEXEC)
  if (pipe2(pfd, O_CLOEXEC) == -1) {
    return -1;
  }
#else  // !HAVE_PIPE2 || !O_CLOEXEC
  if (pipe(pfd) == -1) {
    return -1;
  }
  util::make_socket_closeonexec(pfd[0]);
  util::make_socket_closeonexec(pfd[1]);
#endif // !HAVE_PIPE2 || !O_CLOEXEC

  auto closer = defer([&pfd]() {
    if (pfd[0] != -1) {
      close(pfd[0]);
    }

    if (pfd[1] != -1) {
      close(pfd[1]);
    }
  });

  sigset_t oldset;

  rv = shrpx_signal_block_all(&oldset);
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Blocking all signals failed: errno=" << error;

    return -1;
  }

  auto pid = fork();

  if (pid == 0) {
    // This is multithreaded program, and we are allowed to use only
    // async-signal-safe functions here.

    // child process
    shrpx_signal_unset_worker_proc_ign_handler();

    rv = shrpx_signal_unblock_all();
    if (rv != 0) {
      static constexpr char msg[] = "Unblocking all signals failed\n";
      while (write(STDERR_FILENO, msg, str_size(msg)) == -1 && errno == EINTR)
        ;
      nghttp2_Exit(EXIT_FAILURE);
    }

    dup2(pfd[1], 1);
    close(pfd[0]);

    rv = execv(argv[0], argv);
    if (rv == -1) {
      static constexpr char msg[] = "Could not execute command\n";
      while (write(STDERR_FILENO, msg, str_size(msg)) == -1 && errno == EINTR)
        ;
      nghttp2_Exit(EXIT_FAILURE);
    }
    // unreachable
  }

  // parent process
  if (pid == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not execute command: " << argv[0]
               << ", fork() failed, errno=" << error;
  }

  rv = shrpx_signal_set(&oldset);
  if (rv != 0) {
    auto error = errno;
    LOG(FATAL) << "Restoring all signals failed: errno=" << error;

    nghttp2_Exit(EXIT_FAILURE);
  }

  if (pid == -1) {
    return -1;
  }

  close(pfd[1]);
  pfd[1] = -1;

  util::make_socket_nonblocking(pfd[0]);

  proc.pid = pid;
  proc.rfd = pfd[0];

  pfd[0] = -1;

  return 0;
}

} // namespace shrpx
