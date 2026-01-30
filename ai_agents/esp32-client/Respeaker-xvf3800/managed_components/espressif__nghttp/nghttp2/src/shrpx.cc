/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#include "shrpx.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif // HAVE_SYS_SOCKET_H
#include <sys/un.h>
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H
#include <signal.h>
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // HAVE_NETINET_IN_H
#include <netinet/tcp.h>
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif // HAVE_ARPA_INET_H
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <getopt.h>
#ifdef HAVE_SYSLOG_H
#  include <syslog.h>
#endif // HAVE_SYSLOG_H
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif // HAVE_LIMITS_H
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <sys/resource.h>
#ifdef HAVE_LIBSYSTEMD
#  include <systemd/sd-daemon.h>
#endif // HAVE_LIBSYSTEMD
#ifdef HAVE_LIBBPF
#  include <bpf/libbpf.h>
#endif // HAVE_LIBBPF

#include <cinttypes>
#include <limits>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <initializer_list>
#include <random>
#include <span>

#include "ssl_compat.h"

#ifdef NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <wolfssl/options.h>
#  include <wolfssl/openssl/ssl.h>
#  include <wolfssl/openssl/err.h>
#  include <wolfssl/openssl/rand.h>
#else // !NGHTTP2_OPENSSL_IS_WOLFSSL
#  include <openssl/ssl.h>
#  include <openssl/err.h>
#  include <openssl/rand.h>
#endif // !NGHTTP2_OPENSSL_IS_WOLFSSL
#include <ev.h>

#include <nghttp2/nghttp2.h>

#ifdef ENABLE_HTTP3
#  include <ngtcp2/ngtcp2.h>
#  include <nghttp3/nghttp3.h>
#endif // ENABLE_HTTP3

#include "shrpx_config.h"
#include "shrpx_tls.h"
#include "shrpx_log_config.h"
#include "shrpx_worker.h"
#include "shrpx_http2_upstream.h"
#include "shrpx_http2_session.h"
#include "shrpx_worker_process.h"
#include "shrpx_process.h"
#include "shrpx_signal.h"
#include "shrpx_connection.h"
#include "shrpx_log.h"
#include "shrpx_http.h"
#include "util.h"
#include "app_helper.h"
#include "tls.h"
#include "template.h"
#include "allocator.h"
#include "xsi_strerror.h"

extern char **environ;

using namespace nghttp2;

namespace shrpx {

// Deprecated: Environment variables to tell new binary the listening
// socket's file descriptors.  They are not close-on-exec.
constexpr auto ENV_LISTENER4_FD = "NGHTTPX_LISTENER4_FD"_sr;
constexpr auto ENV_LISTENER6_FD = "NGHTTPX_LISTENER6_FD"_sr;

// Deprecated: Environment variable to tell new binary the port number
// the current binary is listening to.
constexpr auto ENV_PORT = "NGHTTPX_PORT"_sr;

// Deprecated: Environment variable to tell new binary the listening
// socket's file descriptor if frontend listens UNIX domain socket.
constexpr auto ENV_UNIX_FD = "NGHTTP2_UNIX_FD"_sr;
// Deprecated: Environment variable to tell new binary the UNIX domain
// socket path.
constexpr auto ENV_UNIX_PATH = "NGHTTP2_UNIX_PATH"_sr;

// Prefix of environment variables to tell new binary the listening
// socket's file descriptor.  They are not close-on-exec.  For TCP
// socket, the value must be comma separated 2 parameters: tcp,<FD>.
// <FD> is file descriptor.  For UNIX domain socket, the value must be
// comma separated 3 parameters: unix,<FD>,<PATH>.  <FD> is file
// descriptor.  <PATH> is a path to UNIX domain socket.
constexpr auto ENV_ACCEPT_PREFIX = "NGHTTPX_ACCEPT_"_sr;

// This environment variable contains PID of the original main
// process, assuming that it created this main process as a result of
// SIGUSR2.  The new main process is expected to send QUIT signal to
// the original main process to shut it down gracefully.
constexpr auto ENV_ORIG_PID = "NGHTTPX_ORIG_PID"_sr;

// Prefix of environment variables to tell new binary the QUIC IPC
// file descriptor and Worker ID of the lingering worker process.  The
// value must be comma separated parameters:
//
// <FD>,<WORKER_ID_0>,<WORKER_ID_1>,...,<WORKER_ID_I>
//
// <FD> is the file descriptor.  <WORKER_ID_I> is the I-th Worker ID
// in hex encoded string.
constexpr auto ENV_QUIC_WORKER_PROCESS_PREFIX =
  "NGHTTPX_QUIC_WORKER_PROCESS_"_sr;

#ifndef _KERNEL_FASTOPEN
#  define _KERNEL_FASTOPEN
// conditional define for TCP_FASTOPEN mostly on ubuntu
#  ifndef TCP_FASTOPEN
#    define TCP_FASTOPEN 23
#  endif

// conditional define for SOL_TCP mostly on ubuntu
#  ifndef SOL_TCP
#    define SOL_TCP 6
#  endif
#endif

// This configuration is fixed at the first startup of the main
// process, and does not change after subsequent reloadings.
struct StartupConfig {
  // This contains all options given in command-line.
  std::vector<std::pair<StringRef, StringRef>> cmdcfgs;
  // The current working directory where this process started.
  char *cwd;
  // The pointer to original argv (not sure why we have this?)
  char **original_argv;
  // The pointer to argv, this is a deep copy of original argv.
  char **argv;
  // The number of elements in argv.
  int argc;
};

namespace {
StartupConfig suconfig;
} // namespace

struct InheritedAddr {
  // IP address if TCP socket.  Otherwise, UNIX domain socket path.
  StringRef host;
  uint16_t port;
  // true if UNIX domain socket path
  bool host_unix;
  int fd;
  bool used;
};

namespace {
void signal_cb(struct ev_loop *loop, ev_signal *w, int revents);
} // namespace

namespace {
void worker_process_child_cb(struct ev_loop *loop, ev_child *w, int revents);
} // namespace

struct WorkerProcess {
  WorkerProcess(struct ev_loop *loop, pid_t worker_pid, int ipc_fd
#ifdef ENABLE_HTTP3
                ,
                int quic_ipc_fd, std::vector<WorkerID> worker_ids, uint16_t seq
#endif // ENABLE_HTTP3
                )
    : loop(loop),
      worker_pid(worker_pid),
      ipc_fd(ipc_fd)
#ifdef ENABLE_HTTP3
      ,
      quic_ipc_fd(quic_ipc_fd),
      worker_ids(std::move(worker_ids)),
      seq(seq)
#endif // ENABLE_HTTP3
  {
    ev_child_init(&worker_process_childev, worker_process_child_cb, worker_pid,
                  0);
    worker_process_childev.data = this;
    ev_child_start(loop, &worker_process_childev);
  }

  ~WorkerProcess() {
    ev_child_stop(loop, &worker_process_childev);

#ifdef ENABLE_HTTP3
    if (quic_ipc_fd != -1) {
      close(quic_ipc_fd);
    }
#endif // ENABLE_HTTP3

    if (ipc_fd != -1) {
      shutdown(ipc_fd, SHUT_WR);
      close(ipc_fd);
    }
  }

  ev_child worker_process_childev;
  struct ev_loop *loop;
  pid_t worker_pid;
  int ipc_fd;
  std::chrono::steady_clock::time_point termination_deadline;
#ifdef ENABLE_HTTP3
  int quic_ipc_fd;
  std::vector<WorkerID> worker_ids;
  uint16_t seq;
#endif // ENABLE_HTTP3
};

namespace {
void reload_config();
} // namespace

namespace {
std::deque<std::unique_ptr<WorkerProcess>> worker_processes;

#ifdef ENABLE_HTTP3
uint16_t worker_process_seq;
#endif // ENABLE_HTTP3
} // namespace

namespace {
ev_timer worker_process_grace_period_timer;
} // namespace

namespace {
void worker_process_grace_period_timercb(struct ev_loop *loop, ev_timer *w,
                                         int revents) {
  auto now = std::chrono::steady_clock::now();
  auto next_repeat = std::chrono::steady_clock::duration::zero();

  for (auto it = std::begin(worker_processes);
       it != std::end(worker_processes);) {
    auto &wp = *it;
    if (wp->termination_deadline.time_since_epoch().count() == 0) {
      ++it;

      continue;
    }

    auto d = wp->termination_deadline - now;
    if (d.count() > 0) {
      if (next_repeat == std::chrono::steady_clock::duration::zero() ||
          d < next_repeat) {
        next_repeat = d;
      }

      ++it;

      continue;
    }

    LOG(NOTICE) << "Deleting worker process pid=" << wp->worker_pid
                << " because its grace shutdown period is over";

    it = worker_processes.erase(it);
  }

  if (next_repeat.count() > 0) {
    w->repeat = util::ev_tstamp_from(next_repeat);
    ev_timer_again(loop, w);

    return;
  }

  ev_timer_stop(loop, w);
}
} // namespace

namespace {
void worker_process_set_termination_deadline(WorkerProcess *wp,
                                             struct ev_loop *loop) {
  auto config = get_config();

  if (!(config->worker_process_grace_shutdown_period > 0.)) {
    return;
  }

  wp->termination_deadline =
    std::chrono::steady_clock::now() +
    util::duration_from(config->worker_process_grace_shutdown_period);

  if (!ev_is_active(&worker_process_grace_period_timer)) {
    worker_process_grace_period_timer.repeat =
      config->worker_process_grace_shutdown_period;

    ev_timer_again(loop, &worker_process_grace_period_timer);
  }
}
} // namespace

namespace {
void worker_process_add(std::unique_ptr<WorkerProcess> wp) {
  worker_processes.push_back(std::move(wp));
}
} // namespace

namespace {
void worker_process_remove(const WorkerProcess *wp, struct ev_loop *loop) {
  for (auto it = std::begin(worker_processes); it != std::end(worker_processes);
       ++it) {
    auto &s = *it;

    if (s.get() != wp) {
      continue;
    }

    worker_processes.erase(it);

    if (worker_processes.empty()) {
      ev_timer_stop(loop, &worker_process_grace_period_timer);
    }

    break;
  }
}
} // namespace

namespace {
void worker_process_adjust_limit() {
  auto config = get_config();

  if (config->max_worker_processes &&
      worker_processes.size() > config->max_worker_processes) {
    worker_processes.pop_front();
  }
}
} // namespace

namespace {
void worker_process_remove_all(struct ev_loop *loop) {
  std::deque<std::unique_ptr<WorkerProcess>>().swap(worker_processes);

  ev_timer_stop(loop, &worker_process_grace_period_timer);
}
} // namespace

namespace {
// Send signal |signum| to all worker processes, and clears
// worker_processes.
void worker_process_kill(int signum, struct ev_loop *loop) {
  for (auto &s : worker_processes) {
    if (s->worker_pid == -1) {
      continue;
    }
    kill(s->worker_pid, signum);
  }
  worker_process_remove_all(loop);
}
} // namespace

namespace {
int save_pid() {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  auto config = get_config();

  constexpr auto SUFFIX = ".XXXXXX"_sr;
  auto &pid_file = config->pid_file;

  auto len = config->pid_file.size() + SUFFIX.size();
  auto buf = std::make_unique<char[]>(len + 1);
  auto p = buf.get();

  p = std::copy(std::begin(pid_file), std::end(pid_file), p);
  p = std::copy(std::begin(SUFFIX), std::end(SUFFIX), p);
  *p = '\0';

  auto temp_path = buf.get();

  auto fd = mkstemp(temp_path);
  if (fd == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not save PID to file " << pid_file << ": "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  auto content = util::utos(config->pid) + '\n';

  if (write(fd, content.c_str(), content.size()) == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not save PID to file " << pid_file << ": "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  if (fsync(fd) == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not save PID to file " << pid_file << ": "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  close(fd);

  if (rename(temp_path, pid_file.data()) == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not save PID to file " << pid_file << ": "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    unlink(temp_path);

    return -1;
  }

  if (config->uid != 0) {
    if (chown(pid_file.data(), config->uid, config->gid) == -1) {
      auto error = errno;
      LOG(WARN) << "Changing owner of pid file " << pid_file << " failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
  }

  return 0;
}
} // namespace

namespace {
void shrpx_sd_notifyf(int unset_environment, const char *format, ...) {
#ifdef HAVE_LIBSYSTEMD
  va_list args;

  va_start(args, format);
  sd_notifyf(unset_environment, format, va_arg(args, char *));
  va_end(args);
#endif // HAVE_LIBSYSTEMD
}
} // namespace

namespace {
void exec_binary() {
  int rv;
  sigset_t oldset;
  std::array<char, STRERROR_BUFSIZE> errbuf;

  LOG(NOTICE) << "Executing new binary";

  shrpx_sd_notifyf(0, "RELOADING=1");

  rv = shrpx_signal_block_all(&oldset);
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Blocking all signals failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    return;
  }

  auto pid = fork();

  if (pid != 0) {
    if (pid == -1) {
      auto error = errno;
      LOG(ERROR) << "fork() failed errno=" << error;
    } else {
      // update PID tracking information in systemd
      shrpx_sd_notifyf(0, "MAINPID=%d\n", pid);
    }

    rv = shrpx_signal_set(&oldset);

    if (rv != 0) {
      auto error = errno;
      LOG(FATAL) << "Restoring signal mask failed: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());

      exit(EXIT_FAILURE);
    }

    return;
  }

  // child process

  shrpx_signal_unset_main_proc_ign_handler();

  rv = shrpx_signal_unblock_all();
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Unblocking all signals failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    nghttp2_Exit(EXIT_FAILURE);
  }

  auto exec_path =
    util::get_exec_path(suconfig.argc, suconfig.argv, suconfig.cwd);

  if (!exec_path) {
    LOG(ERROR) << "Could not resolve the executable path";
    nghttp2_Exit(EXIT_FAILURE);
  }

  auto argv = std::make_unique<char *[]>(suconfig.argc + 1);

  argv[0] = exec_path;
  for (int i = 1; i < suconfig.argc; ++i) {
    argv[i] = suconfig.argv[i];
  }
  argv[suconfig.argc] = nullptr;

  size_t envlen = 0;
  for (char **p = environ; *p; ++p, ++envlen)
    ;

  auto config = get_config();
  auto &listenerconf = config->conn.listener;

  // 2 for ENV_ORIG_PID and terminal nullptr.
  auto envp = std::make_unique<char *[]>(envlen + listenerconf.addrs.size() +
                                         worker_processes.size() + 2);
  size_t envidx = 0;

  std::vector<ImmutableString> fd_envs;
  for (size_t i = 0; i < listenerconf.addrs.size(); ++i) {
    auto &addr = listenerconf.addrs[i];
    auto s = std::string{ENV_ACCEPT_PREFIX};
    s += util::utos(i + 1);
    s += '=';
    if (addr.host_unix) {
      s += "unix,";
      s += util::utos(addr.fd);
      s += ',';
      s += addr.host;
    } else {
      s += "tcp,";
      s += util::utos(addr.fd);
    }

    fd_envs.emplace_back(s);
    envp[envidx++] = const_cast<char *>(fd_envs.back().c_str());
  }

  auto ipc_fd_str = std::string{ENV_ORIG_PID};
  ipc_fd_str += '=';
  ipc_fd_str += util::utos(config->pid);
  envp[envidx++] = const_cast<char *>(ipc_fd_str.c_str());

#ifdef ENABLE_HTTP3
  std::vector<ImmutableString> quic_lwps;
  for (size_t i = 0; i < worker_processes.size(); ++i) {
    auto &wp = worker_processes[i];
    auto s = std::string{ENV_QUIC_WORKER_PROCESS_PREFIX};
    s += util::utos(i + 1);
    s += '=';
    s += util::utos(wp->quic_ipc_fd);
    for (auto &wid : wp->worker_ids) {
      s += ',';
      s += util::format_hex(std::span{&wid, 1});
    }

    quic_lwps.emplace_back(s);
    envp[envidx++] = const_cast<char *>(quic_lwps.back().c_str());
  }
#endif // ENABLE_HTTP3

  for (size_t i = 0; i < envlen; ++i) {
    auto env = StringRef{environ[i]};
    if (util::starts_with(env, ENV_ACCEPT_PREFIX) ||
        util::starts_with(env, ENV_LISTENER4_FD) ||
        util::starts_with(env, ENV_LISTENER6_FD) ||
        util::starts_with(env, ENV_PORT) ||
        util::starts_with(env, ENV_UNIX_FD) ||
        util::starts_with(env, ENV_UNIX_PATH) ||
        util::starts_with(env, ENV_ORIG_PID) ||
        util::starts_with(env, ENV_QUIC_WORKER_PROCESS_PREFIX)) {
      continue;
    }

    envp[envidx++] = environ[i];
  }

  envp[envidx++] = nullptr;

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "cmdline";
    for (int i = 0; argv[i]; ++i) {
      LOG(INFO) << i << ": " << argv[i];
    }
    LOG(INFO) << "environ";
    for (int i = 0; envp[i]; ++i) {
      LOG(INFO) << i << ": " << envp[i];
    }
  }

  // restores original stderr
  restore_original_fds();

  // reloading finished
  shrpx_sd_notifyf(0, "READY=1");

  if (execve(argv[0], argv.get(), envp.get()) == -1) {
    auto error = errno;
    LOG(ERROR) << "execve failed: errno=" << error;
    nghttp2_Exit(EXIT_FAILURE);
  }
}
} // namespace

namespace {
void ipc_send(WorkerProcess *wp, uint8_t ipc_event) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  ssize_t nwrite;
  while ((nwrite = write(wp->ipc_fd, &ipc_event, 1)) == -1 && errno == EINTR)
    ;

  if (nwrite < 0) {
    auto error = errno;
    LOG(ERROR) << "Could not send IPC event to worker process: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return;
  }

  if (nwrite == 0) {
    LOG(ERROR) << "Could not send IPC event due to pipe overflow";
    return;
  }
}
} // namespace

namespace {
void reopen_log(WorkerProcess *wp) {
  LOG(NOTICE) << "Reopening log files: main process";

  auto config = get_config();
  auto &loggingconf = config->logging;

  (void)reopen_log_files(loggingconf);
  redirect_stderr_to_errorlog(loggingconf);
  ipc_send(wp, SHRPX_IPC_REOPEN_LOG);
}
} // namespace

namespace {
void signal_cb(struct ev_loop *loop, ev_signal *w, int revents) {
  switch (w->signum) {
  case REOPEN_LOG_SIGNAL:
    for (auto &wp : worker_processes) {
      reopen_log(wp.get());
    }

    return;
  case EXEC_BINARY_SIGNAL:
    exec_binary();
    return;
  case GRACEFUL_SHUTDOWN_SIGNAL: {
    auto &listenerconf = get_config()->conn.listener;
    for (auto &addr : listenerconf.addrs) {
      close(addr.fd);
    }

    for (auto &wp : worker_processes) {
      ipc_send(wp.get(), SHRPX_IPC_GRACEFUL_SHUTDOWN);
      worker_process_set_termination_deadline(wp.get(), loop);
    }

    return;
  }
  case RELOAD_SIGNAL:
    reload_config();

    return;
  default:
    worker_process_kill(w->signum, loop);
    ev_break(loop);
    return;
  }
}
} // namespace

namespace {
void worker_process_child_cb(struct ev_loop *loop, ev_child *w, int revents) {
  auto wp = static_cast<WorkerProcess *>(w->data);

  log_chld(w->rpid, w->rstatus, "Worker process");

  worker_process_remove(wp, loop);

  if (worker_processes.empty()) {
    ev_break(loop);
  }
}
} // namespace

namespace {
int create_unix_domain_server_socket(UpstreamAddr &faddr,
                                     std::vector<InheritedAddr> &iaddrs) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  auto found = std::find_if(
    std::begin(iaddrs), std::end(iaddrs), [&faddr](const InheritedAddr &ia) {
      return !ia.used && ia.host_unix && ia.host == faddr.host;
    });

  if (found != std::end(iaddrs)) {
    LOG(NOTICE) << "Listening on UNIX domain socket " << faddr.host
                << (faddr.tls ? ", tls" : "");
    (*found).used = true;
    faddr.fd = (*found).fd;
    faddr.hostport = "localhost"_sr;

    return 0;
  }

#ifdef SOCK_NONBLOCK
  auto fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (fd == -1) {
    auto error = errno;
    LOG(FATAL) << "socket() syscall failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }
#else  // !SOCK_NONBLOCK
  auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    auto error = errno;
    LOG(FATAL) << "socket() syscall failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }
  util::make_socket_nonblocking(fd);
#endif // !SOCK_NONBLOCK
  int val = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                 static_cast<socklen_t>(sizeof(val))) == -1) {
    auto error = errno;
    LOG(FATAL) << "Failed to set SO_REUSEADDR option to listener socket: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    close(fd);
    return -1;
  }

  sockaddr_union addr;
  addr.un.sun_family = AF_UNIX;
  if (faddr.host.size() + 1 > sizeof(addr.un.sun_path)) {
    LOG(FATAL) << "UNIX domain socket path " << faddr.host << " is too long > "
               << sizeof(addr.un.sun_path);
    close(fd);
    return -1;
  }
  // copy path including terminal NULL
  std::copy_n(faddr.host.data(), faddr.host.size() + 1, addr.un.sun_path);

  // unlink (remove) already existing UNIX domain socket path
  unlink(faddr.host.data());

  if (bind(fd, &addr.sa, sizeof(addr.un)) != 0) {
    auto error = errno;
    LOG(FATAL) << "Failed to bind UNIX domain socket: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    close(fd);
    return -1;
  }

  auto &listenerconf = get_config()->conn.listener;

  if (listen(fd, listenerconf.backlog) != 0) {
    auto error = errno;
    LOG(FATAL) << "Failed to listen to UNIX domain socket: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
    close(fd);
    return -1;
  }

  LOG(NOTICE) << "Listening on UNIX domain socket " << faddr.host
              << (faddr.tls ? ", tls" : "");

  faddr.fd = fd;
  faddr.hostport = "localhost"_sr;

  return 0;
}
} // namespace

namespace {
int create_tcp_server_socket(UpstreamAddr &faddr,
                             std::vector<InheritedAddr> &iaddrs) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int fd = -1;
  int rv;

  auto &listenerconf = get_config()->conn.listener;

  auto service = util::utos(faddr.port);
  addrinfo hints{};
  hints.ai_family = faddr.family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
#ifdef AI_ADDRCONFIG
  hints.ai_flags |= AI_ADDRCONFIG;
#endif // AI_ADDRCONFIG

  auto node = faddr.host == "*"_sr ? nullptr : faddr.host.data();

  addrinfo *res, *rp;
  rv = getaddrinfo(node, service.c_str(), &hints, &res);
#ifdef AI_ADDRCONFIG
  if (rv != 0) {
    // Retry without AI_ADDRCONFIG
    hints.ai_flags &= ~AI_ADDRCONFIG;
    rv = getaddrinfo(node, service.c_str(), &hints, &res);
  }
#endif // AI_ADDRCONFIG
  if (rv != 0) {
    LOG(FATAL) << "Unable to get IPv" << (faddr.family == AF_INET ? "4" : "6")
               << " address for " << faddr.host << ", port " << faddr.port
               << ": " << gai_strerror(rv);
    return -1;
  }

  auto res_d = defer(freeaddrinfo, res);

  std::array<char, NI_MAXHOST> host;

  for (rp = res; rp; rp = rp->ai_next) {
    rv = getnameinfo(rp->ai_addr, rp->ai_addrlen, host.data(), host.size(),
                     nullptr, 0, NI_NUMERICHOST);

    if (rv != 0) {
      LOG(WARN) << "getnameinfo() failed: " << gai_strerror(rv);
      continue;
    }

    auto host_sr = StringRef{host.data()};

    auto found = std::find_if(std::begin(iaddrs), std::end(iaddrs),
                              [&host_sr, &faddr](const InheritedAddr &ia) {
                                return !ia.used && !ia.host_unix &&
                                       ia.host == host_sr &&
                                       ia.port == faddr.port;
                              });

    if (found != std::end(iaddrs)) {
      (*found).used = true;
      fd = (*found).fd;
      break;
    }

#ifdef SOCK_NONBLOCK
    fd =
      socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol);
    if (fd == -1) {
      auto error = errno;
      LOG(WARN) << "socket() syscall failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
      continue;
    }
#else  // !SOCK_NONBLOCK
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) {
      auto error = errno;
      LOG(WARN) << "socket() syscall failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
      continue;
    }
    util::make_socket_nonblocking(fd);
#endif // !SOCK_NONBLOCK
    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      auto error = errno;
      LOG(WARN) << "Failed to set SO_REUSEADDR option to listener socket: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
      close(fd);
      continue;
    }

#ifdef IPV6_V6ONLY
    if (faddr.family == AF_INET6) {
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &val,
                     static_cast<socklen_t>(sizeof(val))) == -1) {
        auto error = errno;
        LOG(WARN) << "Failed to set IPV6_V6ONLY option to listener socket: "
                  << xsi_strerror(error, errbuf.data(), errbuf.size());
        close(fd);
        continue;
      }
    }
#endif // IPV6_V6ONLY

#ifdef TCP_DEFER_ACCEPT
    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &val,
                   static_cast<socklen_t>(sizeof(val))) == -1) {
      auto error = errno;
      LOG(WARN) << "Failed to set TCP_DEFER_ACCEPT option to listener socket: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
#endif // TCP_DEFER_ACCEPT

    // When we are executing new binary, and the old binary did not
    // bind privileged port (< 1024) for some reason, binding to those
    // ports will fail with permission denied error.
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == -1) {
      auto error = errno;
      LOG(WARN) << "bind() syscall failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
      close(fd);
      continue;
    }

    if (listenerconf.fastopen > 0) {
      val = listenerconf.fastopen;
      if (setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &val,
                     static_cast<socklen_t>(sizeof(val))) == -1) {
        auto error = errno;
        LOG(WARN) << "Failed to set TCP_FASTOPEN option to listener socket: "
                  << xsi_strerror(error, errbuf.data(), errbuf.size());
      }
    }

    if (listen(fd, listenerconf.backlog) == -1) {
      auto error = errno;
      LOG(WARN) << "listen() syscall failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
      close(fd);
      continue;
    }

    break;
  }

  if (!rp) {
    LOG(FATAL) << "Listening " << (faddr.family == AF_INET ? "IPv4" : "IPv6")
               << " socket failed";

    return -1;
  }

  faddr.fd = fd;
  faddr.hostport = util::make_http_hostport(mod_config()->balloc,
                                            StringRef{host.data()}, faddr.port);

  LOG(NOTICE) << "Listening on " << faddr.hostport
              << (faddr.tls ? ", tls" : "");

  return 0;
}
} // namespace

namespace {
// Returns array of InheritedAddr constructed from |config|.  This
// function is intended to be used when reloading configuration, and
// |config| is usually a current configuration.
std::vector<InheritedAddr>
get_inherited_addr_from_config(BlockAllocator &balloc, Config *config) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;

  auto &listenerconf = config->conn.listener;

  std::vector<InheritedAddr> iaddrs(listenerconf.addrs.size());

  size_t idx = 0;
  for (auto &addr : listenerconf.addrs) {
    auto &iaddr = iaddrs[idx++];

    if (addr.host_unix) {
      iaddr.host = addr.host;
      iaddr.host_unix = true;
      iaddr.fd = addr.fd;

      continue;
    }

    iaddr.port = addr.port;
    iaddr.fd = addr.fd;

    // We have to getsockname/getnameinfo for fd, since we may have
    // '*' appear in addr.host, which makes comparison against "real"
    // address fail.

    sockaddr_union su;
    socklen_t salen = sizeof(su);

    // We already added entry to iaddrs.  Even if we got errors, we
    // don't remove it.  This is required because we have to close the
    // socket if it is not reused.  The empty host name usually does
    // not match anything.

    if (getsockname(addr.fd, &su.sa, &salen) != 0) {
      auto error = errno;
      LOG(WARN) << "getsockname() syscall failed (fd=" << addr.fd
                << "): " << xsi_strerror(error, errbuf.data(), errbuf.size());
      continue;
    }

    std::array<char, NI_MAXHOST> host;
    rv = getnameinfo(&su.sa, salen, host.data(), host.size(), nullptr, 0,
                     NI_NUMERICHOST);
    if (rv != 0) {
      LOG(WARN) << "getnameinfo() failed (fd=" << addr.fd
                << "): " << gai_strerror(rv);
      continue;
    }

    iaddr.host = make_string_ref(balloc, StringRef{host.data()});
  }

  return iaddrs;
}
} // namespace

namespace {
// Returns array of InheritedAddr constructed from environment
// variables.  This function handles the old environment variable
// names used in 1.7.0 or earlier.
std::vector<InheritedAddr> get_inherited_addr_from_env(Config *config) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;
  std::vector<InheritedAddr> iaddrs;

  {
    // Upgrade from 1.7.0 or earlier
    auto portenv = getenv(ENV_PORT.data());
    if (portenv) {
      size_t i = 1;
      for (const auto &env_name : {ENV_LISTENER4_FD, ENV_LISTENER6_FD}) {
        auto fdenv = getenv(env_name.data());
        if (fdenv) {
          auto name = std::string{ENV_ACCEPT_PREFIX};
          name += util::utos(i);
          std::string value = "tcp,";
          value += fdenv;
          setenv(name.c_str(), value.c_str(), 0);
          ++i;
        }
      }
    } else {
      // The return value of getenv may be allocated statically.
      if (getenv(ENV_UNIX_PATH.data()) && getenv(ENV_UNIX_FD.data())) {
        auto name = std::string{ENV_ACCEPT_PREFIX};
        name += '1';
        std::string value = "unix,";
        value += getenv(ENV_UNIX_FD.data());
        value += ',';
        value += getenv(ENV_UNIX_PATH.data());
        setenv(name.c_str(), value.c_str(), 0);
      }
    }
  }

  for (size_t i = 1;; ++i) {
    auto name = std::string{ENV_ACCEPT_PREFIX};
    name += util::utos(i);
    auto env = getenv(name.c_str());
    if (!env) {
      break;
    }

    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Read env " << name << "=" << env;
    }

    auto end_type = strchr(env, ',');
    if (!end_type) {
      continue;
    }

    auto type = StringRef(env, end_type);
    auto value = end_type + 1;

    if (type == "unix"_sr) {
      auto endfd = strchr(value, ',');
      if (!endfd) {
        continue;
      }
      auto fd = util::parse_uint(StringRef{value, endfd});
      if (!fd) {
        LOG(WARN) << "Could not parse file descriptor from "
                  << std::string(value, endfd - value);
        continue;
      }

      auto path = endfd + 1;
      if (strlen(path) == 0) {
        LOG(WARN) << "Empty UNIX domain socket path (fd=" << *fd << ")";
        close(*fd);
        continue;
      }

      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "Inherit UNIX domain socket fd=" << *fd
                  << ", path=" << path;
      }

      InheritedAddr addr{};
      addr.host = make_string_ref(config->balloc, StringRef{path});
      addr.host_unix = true;
      addr.fd = static_cast<int>(*fd);
      iaddrs.push_back(std::move(addr));
    }

    if (type == "tcp"_sr) {
      auto fd = util::parse_uint(value);
      if (!fd) {
        LOG(WARN) << "Could not parse file descriptor from " << value;
        continue;
      }

      sockaddr_union su;
      socklen_t salen = sizeof(su);

      if (getsockname(*fd, &su.sa, &salen) != 0) {
        auto error = errno;
        LOG(WARN) << "getsockname() syscall failed (fd=" << *fd
                  << "): " << xsi_strerror(error, errbuf.data(), errbuf.size());
        close(*fd);
        continue;
      }

      uint16_t port;

      switch (su.storage.ss_family) {
      case AF_INET:
        port = ntohs(su.in.sin_port);
        break;
      case AF_INET6:
        port = ntohs(su.in6.sin6_port);
        break;
      default:
        close(*fd);
        continue;
      }

      std::array<char, NI_MAXHOST> host;
      rv = getnameinfo(&su.sa, salen, host.data(), host.size(), nullptr, 0,
                       NI_NUMERICHOST);
      if (rv != 0) {
        LOG(WARN) << "getnameinfo() failed (fd=" << *fd
                  << "): " << gai_strerror(rv);
        close(*fd);
        continue;
      }

      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "Inherit TCP socket fd=" << *fd
                  << ", address=" << host.data() << ", port=" << port;
      }

      InheritedAddr addr{};
      addr.host = make_string_ref(config->balloc, StringRef{host.data()});
      addr.port = static_cast<uint16_t>(port);
      addr.fd = static_cast<int>(*fd);
      iaddrs.push_back(std::move(addr));
      continue;
    }
  }

  return iaddrs;
}
} // namespace

namespace {
// Closes all sockets which are not reused.
void close_unused_inherited_addr(const std::vector<InheritedAddr> &iaddrs) {
  for (auto &ia : iaddrs) {
    if (ia.used) {
      continue;
    }

    close(ia.fd);
  }
}
} // namespace

namespace {
// Returns the PID of the original main process from environment
// variable ENV_ORIG_PID.
pid_t get_orig_pid_from_env() {
  auto s = getenv(ENV_ORIG_PID.data());
  if (s == nullptr) {
    return -1;
  }
  return util::parse_uint(s).value_or(-1);
}
} // namespace

#ifdef ENABLE_HTTP3
namespace {
std::vector<QUICLingeringWorkerProcess>
  inherited_quic_lingering_worker_processes;
} // namespace

namespace {
std::vector<QUICLingeringWorkerProcess>
get_inherited_quic_lingering_worker_process_from_env() {
  std::vector<QUICLingeringWorkerProcess> lwps;

  for (size_t i = 1;; ++i) {
    auto name = std::string{ENV_QUIC_WORKER_PROCESS_PREFIX};
    name += util::utos(i);
    auto env = getenv(name.c_str());
    if (!env) {
      break;
    }

    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Read env " << name << "=" << env;
    }

    auto envend = env + strlen(env);

    auto end_fd = std::find(env, envend, ',');
    if (end_fd == envend) {
      continue;
    }

    auto fd = util::parse_uint(StringRef{env, end_fd});
    if (!fd) {
      LOG(WARN) << "Could not parse file descriptor from "
                << StringRef{env, static_cast<size_t>(end_fd - env)};
      continue;
    }

    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Inherit worker process QUIC IPC socket fd=" << *fd;
    }

    util::make_socket_closeonexec(*fd);

    std::vector<WorkerID> worker_ids;

    auto p = end_fd + 1;
    for (;;) {
      auto end = std::find(p, envend, ',');

      auto hex_wid = StringRef{p, end};
      if (hex_wid.size() != SHRPX_QUIC_WORKER_IDLEN * 2 ||
          !util::is_hex_string(hex_wid)) {
        LOG(WARN) << "Found invalid WorkerID=" << hex_wid;
        break;
      }

      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "Inherit worker process WorkerID=" << hex_wid;
      }

      worker_ids.emplace_back();

      util::decode_hex(reinterpret_cast<uint8_t *>(&worker_ids.back()),
                       hex_wid);

      if (end == envend) {
        break;
      }

      p = end + 1;
    }

    lwps.emplace_back(std::move(worker_ids), *fd);
  }

  if (!lwps.empty()) {
    const auto &lwp = lwps.back();

    if (!lwp.worker_ids.empty() &&
        worker_process_seq <= lwp.worker_ids[0].worker_process) {
      worker_process_seq = lwp.worker_ids[0].worker_process;
      ++worker_process_seq;
    }
  }

  return lwps;
}
} // namespace
#endif // ENABLE_HTTP3

namespace {
int create_acceptor_socket(Config *config, std::vector<InheritedAddr> &iaddrs) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  auto &listenerconf = config->conn.listener;

  for (auto &addr : listenerconf.addrs) {
    if (addr.host_unix) {
      if (create_unix_domain_server_socket(addr, iaddrs) != 0) {
        return -1;
      }

      if (config->uid != 0) {
        // fd is not associated to inode, so we cannot use fchown(2)
        // here.  https://lkml.org/lkml/2004/11/1/84
        if (chown(addr.host.data(), config->uid, config->gid) == -1) {
          auto error = errno;
          LOG(WARN) << "Changing owner of UNIX domain socket " << addr.host
                    << " failed: "
                    << xsi_strerror(error, errbuf.data(), errbuf.size());
        }
      }
      continue;
    }

    if (create_tcp_server_socket(addr, iaddrs) != 0) {
      return -1;
    }
  }

  return 0;
}
} // namespace

namespace {
int call_daemon() {
#ifdef __sgi
  return _daemonize(0, 0, 0, 0);
#else // !__sgi
#  ifdef HAVE_LIBSYSTEMD
  if (sd_booted() && (getenv("NOTIFY_SOCKET") != nullptr)) {
    LOG(NOTICE) << "Daemonising disabled under systemd";
    chdir("/");
    return 0;
  }
#  endif // HAVE_LIBSYSTEMD
  return util::daemonize(0, 0);
#endif   // !__sgi
}
} // namespace

namespace {
// Opens IPC socket used to communicate with worker proess.  The
// communication is unidirectional; that is main process sends
// messages to the worker process.  On success, ipc_fd[0] is for
// reading, and ipc_fd[1] for writing, just like pipe(2).
int create_ipc_socket(std::span<int, 2> ipc_fd) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;

  rv = pipe(ipc_fd.data());
  if (rv == -1) {
    auto error = errno;
    LOG(WARN) << "Failed to create pipe to communicate worker process: "
              << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  for (auto fd : ipc_fd) {
    util::make_socket_nonblocking(fd);
    util::make_socket_closeonexec(fd);
  }

  return 0;
}
} // namespace

namespace {
int create_worker_process_ready_ipc_socket(std::span<int, 2> ipc_fd) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;

  rv = socketpair(AF_UNIX, SOCK_DGRAM, 0, ipc_fd.data());
  if (rv == -1) {
    auto error = errno;
    LOG(WARN) << "Failed to create socket pair to communicate worker process "
                 "readiness: "
              << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  for (auto fd : ipc_fd) {
    util::make_socket_closeonexec(fd);
  }

  util::make_socket_nonblocking(ipc_fd[0]);

  return 0;
}
} // namespace

#ifdef ENABLE_HTTP3
namespace {
int create_quic_ipc_socket(std::span<int, 2> quic_ipc_fd) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;

  rv = socketpair(AF_UNIX, SOCK_DGRAM, 0, quic_ipc_fd.data());
  if (rv == -1) {
    auto error = errno;
    LOG(WARN) << "Failed to create socket pair to communicate worker process: "
              << xsi_strerror(error, errbuf.data(), errbuf.size());
    return -1;
  }

  for (auto fd : quic_ipc_fd) {
    util::make_socket_nonblocking(fd);
  }

  return 0;
}
} // namespace

namespace {
int generate_worker_id(std::vector<WorkerID> &worker_ids, uint16_t wp_seq,
                       const Config *config) {
  auto &apiconf = config->api;
  auto &quicconf = config->quic;

  size_t num_wid;
  if (config->single_thread) {
    num_wid = 1;
  } else {
    num_wid = config->num_worker;

    // API endpoint occupies the one dedicated worker thread.
    // Although such worker never gets QUIC traffic, we create Worker
    // ID for it to make code a bit simpler.
    if (apiconf.enabled) {
      ++num_wid;
    }
  }

  worker_ids.resize(num_wid);

  uint16_t idx = 0;

  for (auto &wid : worker_ids) {
    wid.server = quicconf.server_id;
    wid.worker_process = wp_seq;
    wid.thread = idx++;
  }

  return 0;
}
} // namespace

namespace {
std::vector<QUICLingeringWorkerProcess>
collect_quic_lingering_worker_processes() {
  std::vector<QUICLingeringWorkerProcess> quic_lwps{
    std::begin(inherited_quic_lingering_worker_processes),
    std::end(inherited_quic_lingering_worker_processes)};

  for (auto &wp : worker_processes) {
    quic_lwps.emplace_back(wp->worker_ids, wp->quic_ipc_fd);
  }

  return quic_lwps;
}
} // namespace
#endif // ENABLE_HTTP3

namespace {
ev_signal reopen_log_signalev;
ev_signal exec_binary_signalev;
ev_signal graceful_shutdown_signalev;
ev_signal reload_signalev;
} // namespace

namespace {
void start_signal_watchers(struct ev_loop *loop) {
  ev_signal_init(&reopen_log_signalev, signal_cb, REOPEN_LOG_SIGNAL);
  ev_signal_start(loop, &reopen_log_signalev);

  ev_signal_init(&exec_binary_signalev, signal_cb, EXEC_BINARY_SIGNAL);
  ev_signal_start(loop, &exec_binary_signalev);

  ev_signal_init(&graceful_shutdown_signalev, signal_cb,
                 GRACEFUL_SHUTDOWN_SIGNAL);
  ev_signal_start(loop, &graceful_shutdown_signalev);

  ev_signal_init(&reload_signalev, signal_cb, RELOAD_SIGNAL);
  ev_signal_start(loop, &reload_signalev);
}
} // namespace

namespace {
void shutdown_signal_watchers(struct ev_loop *loop) {
  ev_signal_stop(loop, &reload_signalev);
  ev_signal_stop(loop, &graceful_shutdown_signalev);
  ev_signal_stop(loop, &exec_binary_signalev);
  ev_signal_stop(loop, &reopen_log_signalev);
}
} // namespace

namespace {
// A pair of connected socket with which a worker process tells main
// process that it is ready for service.  A worker process writes its
// PID to worker_process_ready_ipc_fd[1] and main process reads it
// from worker_process_ready_ipc_fd[0].
std::array<int, 2> worker_process_ready_ipc_fd;
} // namespace

namespace {
ev_io worker_process_ready_ipcev;
} // namespace

namespace {
// PID received via NGHTTPX_ORIG_PID environment variable.
pid_t orig_pid = -1;
} // namespace

namespace {
void worker_process_ready_ipc_readcb(struct ev_loop *loop, ev_io *w,
                                     int revents) {
  std::array<uint8_t, 8> buf;
  ssize_t nread;

  while ((nread = read(w->fd, buf.data(), buf.size())) == -1 && errno == EINTR)
    ;

  if (nread == -1) {
    std::array<char, STRERROR_BUFSIZE> errbuf;
    auto error = errno;

    LOG(ERROR) << "Failed to read data from worker process ready IPC channel: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    return;
  }

  if (nread == 0) {
    return;
  }

  if (nread != sizeof(pid_t)) {
    LOG(ERROR) << "Read " << nread
               << " bytes from worker process ready IPC channel";

    return;
  }

  pid_t pid;

  memcpy(&pid, buf.data(), sizeof(pid));

  LOG(NOTICE) << "Worker process pid=" << pid << " is ready";

  for (auto &wp : worker_processes) {
    // Send graceful shutdown signal to all worker processes prior to
    // pid.
    if (wp->worker_pid == pid) {
      break;
    }

    LOG(INFO) << "Sending graceful shutdown event to worker process pid="
              << wp->worker_pid;

    ipc_send(wp.get(), SHRPX_IPC_GRACEFUL_SHUTDOWN);
    worker_process_set_termination_deadline(wp.get(), loop);
  }

  if (orig_pid != -1) {
    LOG(NOTICE) << "Send QUIT signal to the original main process to tell "
                   "that we are ready to serve requests.";
    kill(orig_pid, SIGQUIT);

    orig_pid = -1;
  }
}
} // namespace

namespace {
void start_worker_process_ready_ipc_watcher(struct ev_loop *loop) {
  ev_io_init(&worker_process_ready_ipcev, worker_process_ready_ipc_readcb,
             worker_process_ready_ipc_fd[0], EV_READ);
  ev_io_start(loop, &worker_process_ready_ipcev);
}
} // namespace

namespace {
void shutdown_worker_process_ready_ipc_watcher(struct ev_loop *loop) {
  ev_io_stop(loop, &worker_process_ready_ipcev);
}
} // namespace

namespace {
// Creates worker process, and returns PID of worker process.  On
// success, file descriptor for IPC (send only) is assigned to
// |main_ipc_fd|.  In child process, we will close file descriptors
// which are inherited from previous configuration/process, but not
// used in the current configuration.
pid_t fork_worker_process(int &main_ipc_fd
#ifdef ENABLE_HTTP3
                          ,
                          int &wp_quic_ipc_fd
#endif // ENABLE_HTTP3
                          ,
                          const std::vector<InheritedAddr> &iaddrs
#ifdef ENABLE_HTTP3
                          ,
                          std::vector<WorkerID> worker_ids,
                          std::vector<QUICLingeringWorkerProcess> quic_lwps
#endif // ENABLE_HTTP3
) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  int rv;
  sigset_t oldset;

  std::array<int, 2> ipc_fd;

  rv = create_ipc_socket(ipc_fd);
  if (rv != 0) {
    return -1;
  }

#ifdef ENABLE_HTTP3
  std::array<int, 2> quic_ipc_fd;

  rv = create_quic_ipc_socket(quic_ipc_fd);
  if (rv != 0) {
    return -1;
  }
#endif // ENABLE_HTTP3

  rv = shrpx_signal_block_all(&oldset);
  if (rv != 0) {
    auto error = errno;
    LOG(ERROR) << "Blocking all signals failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    close(ipc_fd[0]);
    close(ipc_fd[1]);

    return -1;
  }

  auto config = get_config();

  pid_t pid = 0;

  if (!config->single_process) {
    pid = fork();
  }

  if (pid == 0) {
    // We are in new process now, update pid for logger.
    log_config()->pid = getpid();

    ev_loop_fork(EV_DEFAULT);

    for (auto &addr : config->conn.listener.addrs) {
      util::make_socket_closeonexec(addr.fd);
    }

#ifdef ENABLE_HTTP3
    util::make_socket_closeonexec(quic_ipc_fd[0]);

    for (auto &lwp : quic_lwps) {
      util::make_socket_closeonexec(lwp.quic_ipc_fd);
    }

    for (auto &wp : worker_processes) {
      util::make_socket_closeonexec(wp->quic_ipc_fd);
      // Do not close quic_ipc_fd.
      wp->quic_ipc_fd = -1;
    }
#endif // ENABLE_HTTP3

    if (!config->single_process) {
      close(worker_process_ready_ipc_fd[0]);
      shutdown_worker_process_ready_ipc_watcher(EV_DEFAULT);

      shutdown_signal_watchers(EV_DEFAULT);
    }

    // Remove all WorkerProcesses to stop any registered watcher on
    // default loop.
    worker_process_remove_all(EV_DEFAULT);

    close_unused_inherited_addr(iaddrs);

    shrpx_signal_set_worker_proc_ign_handler();

    rv = shrpx_signal_unblock_all();
    if (rv != 0) {
      auto error = errno;
      LOG(FATAL) << "Unblocking all signals failed: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());

      if (config->single_process) {
        exit(EXIT_FAILURE);
      } else {
        nghttp2_Exit(EXIT_FAILURE);
      }
    }

    if (!config->single_process) {
      close(ipc_fd[1]);
#ifdef ENABLE_HTTP3
      close(quic_ipc_fd[1]);
#endif // ENABLE_HTTP3
    }

    WorkerProcessConfig wpconf{
      .ipc_fd = ipc_fd[0],
      .ready_ipc_fd = worker_process_ready_ipc_fd[1],
#ifdef ENABLE_HTTP3
      .worker_ids = std::move(worker_ids),
      .quic_ipc_fd = quic_ipc_fd[0],
      .quic_lingering_worker_processes = std::move(quic_lwps),
#endif // ENABLE_HTTP3
    };
    rv = worker_process_event_loop(&wpconf);
    if (rv != 0) {
      LOG(FATAL) << "Worker process returned error";

      if (config->single_process) {
        exit(EXIT_FAILURE);
      } else {
        nghttp2_Exit(EXIT_FAILURE);
      }
    }

    LOG(NOTICE) << "Worker process shutting down momentarily";

    // call exit(...) instead of nghttp2_Exit to get leak sanitizer report
    if (config->single_process) {
      exit(EXIT_SUCCESS);
    } else {
      nghttp2_Exit(EXIT_SUCCESS);
    }
  }

  // parent process
  if (pid == -1) {
    auto error = errno;
    LOG(ERROR) << "Could not spawn worker process: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());
  }

  rv = shrpx_signal_set(&oldset);
  if (rv != 0) {
    auto error = errno;
    LOG(FATAL) << "Restoring signal mask failed: "
               << xsi_strerror(error, errbuf.data(), errbuf.size());

    exit(EXIT_FAILURE);
  }

  if (pid == -1) {
    close(ipc_fd[0]);
    close(ipc_fd[1]);
#ifdef ENABLE_HTTP3
    close(quic_ipc_fd[0]);
    close(quic_ipc_fd[1]);
#endif // ENABLE_HTTP3

    return -1;
  }

  close(ipc_fd[0]);
#ifdef ENABLE_HTTP3
  close(quic_ipc_fd[0]);
#endif // ENABLE_HTTP3

  main_ipc_fd = ipc_fd[1];
#ifdef ENABLE_HTTP3
  wp_quic_ipc_fd = quic_ipc_fd[1];
#endif // ENABLE_HTTP3

  LOG(NOTICE) << "Worker process [" << pid << "] spawned";

  return pid;
}
} // namespace

namespace {
int event_loop() {
  std::array<char, STRERROR_BUFSIZE> errbuf;

  shrpx_signal_set_main_proc_ign_handler();

  auto config = mod_config();

  if (config->daemon) {
    if (call_daemon() == -1) {
      auto error = errno;
      LOG(FATAL) << "Failed to daemonize: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());
      return -1;
    }

    // We get new PID after successful daemon().
    mod_config()->pid = getpid();

    // daemon redirects stderr file descriptor to /dev/null, so we
    // need this.
    redirect_stderr_to_errorlog(config->logging);
  }

  // update systemd PID tracking
  shrpx_sd_notifyf(0, "MAINPID=%d\n", config->pid);

  {
    auto iaddrs = get_inherited_addr_from_env(config);

    if (create_acceptor_socket(config, iaddrs) != 0) {
      return -1;
    }

    close_unused_inherited_addr(iaddrs);
  }

  orig_pid = get_orig_pid_from_env();

#ifdef ENABLE_HTTP3
  inherited_quic_lingering_worker_processes =
    get_inherited_quic_lingering_worker_process_from_env();
#endif // ENABLE_HTTP3

  auto loop = ev_default_loop(config->ev_loop_flags);

  int ipc_fd = 0;
#ifdef ENABLE_HTTP3
  int quic_ipc_fd = 0;

  auto quic_lwps = collect_quic_lingering_worker_processes();

  std::vector<WorkerID> worker_ids;

  if (generate_worker_id(worker_ids, worker_process_seq, config) != 0) {
    return -1;
  }
#endif // ENABLE_HTTP3

  if (!config->single_process) {
    start_signal_watchers(loop);
  }

  create_worker_process_ready_ipc_socket(worker_process_ready_ipc_fd);
  start_worker_process_ready_ipc_watcher(loop);

  auto pid = fork_worker_process(ipc_fd
#ifdef ENABLE_HTTP3
                                 ,
                                 quic_ipc_fd
#endif // ENABLE_HTTP3
                                 ,
                                 {}
#ifdef ENABLE_HTTP3
                                 ,
                                 worker_ids, std::move(quic_lwps)
#endif // ENABLE_HTTP3
  );

  if (pid == -1) {
    return -1;
  }

  ev_timer_init(&worker_process_grace_period_timer,
                worker_process_grace_period_timercb, 0., 0.);

  worker_process_add(std::make_unique<WorkerProcess>(
    loop, pid, ipc_fd
#ifdef ENABLE_HTTP3
    ,
    quic_ipc_fd, std::move(worker_ids), worker_process_seq++
#endif // ENABLE_HTTP3
    ));

  // Write PID file when we are ready to accept connection from peer.
  // This makes easier to write restart script for nghttpx.  Because
  // when we know that PID file is recreated, it means we can send
  // QUIT signal to the old process to make it shutdown gracefully.
  if (!config->pid_file.empty()) {
    save_pid();
  }

  shrpx_sd_notifyf(0, "READY=1");

  ev_run(loop, 0);

  ev_timer_stop(loop, &worker_process_grace_period_timer);

  shutdown_worker_process_ready_ipc_watcher(loop);

  // config is now stale if reload has happened.
  if (!get_config()->single_process) {
    shutdown_signal_watchers(loop);
  }

  return 0;
}
} // namespace

namespace {
// Returns true if regular file or symbolic link |path| exists.
bool conf_exists(const char *path) {
  struct stat buf;
  int rv = stat(path, &buf);
  return rv == 0 && (buf.st_mode & (S_IFREG | S_IFLNK));
}
} // namespace

namespace {
constexpr auto DEFAULT_ALPN_LIST = "h2,h2-16,h2-14,http/1.1"_sr;
} // namespace

namespace {
constexpr auto DEFAULT_TLS_MIN_PROTO_VERSION = "TLSv1.2"_sr;
#ifdef TLS1_3_VERSION
constexpr auto DEFAULT_TLS_MAX_PROTO_VERSION = "TLSv1.3"_sr;
#else  // !TLS1_3_VERSION
constexpr auto DEFAULT_TLS_MAX_PROTO_VERSION = "TLSv1.2"_sr;
#endif // !TLS1_3_VERSION
} // namespace

namespace {
constexpr auto DEFAULT_ACCESSLOG_FORMAT =
  R"($remote_addr - - [$time_local] )"
  R"("$request" $status $body_bytes_sent )"
  R"("$http_referer" "$http_user_agent")"_sr;
} // namespace

namespace {
void fill_default_config(Config *config) {
  config->num_worker = 1;
  config->conf_path = "/etc/nghttpx/nghttpx.conf"_sr;
  config->pid = getpid();

#ifdef NOTHREADS
  config->single_thread = true;
#endif // NOTHREADS

  if (ev_supported_backends() & ~ev_recommended_backends() & EVBACKEND_KQUEUE) {
    config->ev_loop_flags = ev_recommended_backends() | EVBACKEND_KQUEUE;
  }

  auto &tlsconf = config->tls;
  {
    auto &ticketconf = tlsconf.ticket;
    {
      auto &memcachedconf = ticketconf.memcached;
      memcachedconf.max_retry = 3;
      memcachedconf.max_fail = 2;
      memcachedconf.interval = 10_min;
      memcachedconf.family = AF_UNSPEC;
    }

    auto &session_cacheconf = tlsconf.session_cache;
    {
      auto &memcachedconf = session_cacheconf.memcached;
      memcachedconf.family = AF_UNSPEC;
    }

    ticketconf.cipher = EVP_aes_128_cbc();
  }

  {
    auto &ocspconf = tlsconf.ocsp;
    // ocsp update interval = 14400 secs = 4 hours, borrowed from h2o
    ocspconf.update_interval = 4_h;
    ocspconf.fetch_ocsp_response_file = PKGDATADIR "/fetch-ocsp-response"_sr;
  }

  {
    auto &dyn_recconf = tlsconf.dyn_rec;
    dyn_recconf.warmup_threshold = 1_m;
    dyn_recconf.idle_timeout = 1_s;
  }

  tlsconf.session_timeout = std::chrono::hours(12);
  tlsconf.ciphers = StringRef{nghttp2::tls::DEFAULT_CIPHER_LIST};
  tlsconf.tls13_ciphers = StringRef{nghttp2::tls::DEFAULT_TLS13_CIPHER_LIST};
  tlsconf.client.ciphers = StringRef{nghttp2::tls::DEFAULT_CIPHER_LIST};
  tlsconf.client.tls13_ciphers =
    StringRef{nghttp2::tls::DEFAULT_TLS13_CIPHER_LIST};
  tlsconf.min_proto_version =
    tls::proto_version_from_string(DEFAULT_TLS_MIN_PROTO_VERSION);
  tlsconf.max_proto_version =
    tls::proto_version_from_string(DEFAULT_TLS_MAX_PROTO_VERSION);
  tlsconf.max_early_data = 16_k;
  tlsconf.ecdh_curves = "X25519:P-256:P-384:P-521"_sr;

  auto &httpconf = config->http;
  httpconf.server_name = "nghttpx"_sr;
  httpconf.no_host_rewrite = true;
  httpconf.request_header_field_buffer = 64_k;
  httpconf.max_request_header_fields = 100;
  httpconf.response_header_field_buffer = 64_k;
  httpconf.max_response_header_fields = 500;
  httpconf.redirect_https_port = "443"_sr;
  httpconf.max_requests = std::numeric_limits<size_t>::max();
  httpconf.xfp.add = true;
  httpconf.xfp.strip_incoming = true;
  httpconf.early_data.strip_incoming = true;
  httpconf.timeout.header = 1_min;

  auto &http2conf = config->http2;
  {
    auto &upstreamconf = http2conf.upstream;

    {
      auto &timeoutconf = upstreamconf.timeout;
      timeoutconf.settings = 10_s;
    }

    // window size for HTTP/2 upstream connection per stream.  2**16-1
    // = 64KiB-1, which is HTTP/2 default.
    upstreamconf.window_size = 64_k - 1;
    // HTTP/2 has connection-level flow control. The default window
    // size for HTTP/2 is 64KiB - 1.
    upstreamconf.connection_window_size = 64_k - 1;
    upstreamconf.max_concurrent_streams = 100;

    upstreamconf.encoder_dynamic_table_size = 4_k;
    upstreamconf.decoder_dynamic_table_size = 4_k;

    nghttp2_option_new(&upstreamconf.option);
    nghttp2_option_set_no_auto_window_update(upstreamconf.option, 1);
    nghttp2_option_set_no_recv_client_magic(upstreamconf.option, 1);
    nghttp2_option_set_max_deflate_dynamic_table_size(
      upstreamconf.option, upstreamconf.encoder_dynamic_table_size);
    nghttp2_option_set_server_fallback_rfc7540_priorities(upstreamconf.option,
                                                          1);
    nghttp2_option_set_builtin_recv_extension_type(upstreamconf.option,
                                                   NGHTTP2_PRIORITY_UPDATE);

    // For API endpoint, we enable automatic window update.  This is
    // because we are a sink.
    nghttp2_option_new(&upstreamconf.alt_mode_option);
    nghttp2_option_set_no_recv_client_magic(upstreamconf.alt_mode_option, 1);
    nghttp2_option_set_max_deflate_dynamic_table_size(
      upstreamconf.alt_mode_option, upstreamconf.encoder_dynamic_table_size);
  }

  http2conf.timeout.stream_write = 1_min;

  {
    auto &downstreamconf = http2conf.downstream;

    {
      auto &timeoutconf = downstreamconf.timeout;
      timeoutconf.settings = 10_s;
    }

    downstreamconf.window_size = 64_k - 1;
    downstreamconf.connection_window_size = (1u << 31) - 1;
    downstreamconf.max_concurrent_streams = 100;

    downstreamconf.encoder_dynamic_table_size = 4_k;
    downstreamconf.decoder_dynamic_table_size = 4_k;

    nghttp2_option_new(&downstreamconf.option);
    nghttp2_option_set_no_auto_window_update(downstreamconf.option, 1);
    nghttp2_option_set_peer_max_concurrent_streams(downstreamconf.option, 100);
    nghttp2_option_set_max_deflate_dynamic_table_size(
      downstreamconf.option, downstreamconf.encoder_dynamic_table_size);
  }

#ifdef ENABLE_HTTP3
  auto &quicconf = config->quic;
  {
    auto &upstreamconf = quicconf.upstream;

    {
      auto &timeoutconf = upstreamconf.timeout;
      timeoutconf.idle = 30_s;
    }

    auto &bpfconf = quicconf.bpf;
    bpfconf.prog_file = PKGLIBDIR "/reuseport_kern.o"_sr;

    upstreamconf.congestion_controller = NGTCP2_CC_ALGO_CUBIC;

    upstreamconf.initial_rtt =
      static_cast<ev_tstamp>(NGTCP2_DEFAULT_INITIAL_RTT) / NGTCP2_SECONDS;
  }

  if (RAND_bytes(reinterpret_cast<unsigned char *>(&quicconf.server_id),
                 sizeof(quicconf.server_id)) != 1) {
    assert(0);
    abort();
  }

  auto &http3conf = config->http3;
  {
    auto &upstreamconf = http3conf.upstream;

    upstreamconf.max_concurrent_streams = 100;
    upstreamconf.window_size = 256_k;
    upstreamconf.connection_window_size = 1_m;
    upstreamconf.max_window_size = 6_m;
    upstreamconf.max_connection_window_size = 8_m;
  }
#endif // ENABLE_HTTP3

  auto &loggingconf = config->logging;
  {
    auto &accessconf = loggingconf.access;
    accessconf.format =
      parse_log_format(config->balloc, DEFAULT_ACCESSLOG_FORMAT);

    auto &errorconf = loggingconf.error;
    errorconf.file = "/dev/stderr"_sr;
  }

  loggingconf.syslog_facility = LOG_DAEMON;
  loggingconf.severity = NOTICE;

  auto &connconf = config->conn;
  {
    auto &listenerconf = connconf.listener;
    {
      // Default accept() backlog
      listenerconf.backlog = 65536;
      listenerconf.timeout.sleep = 30_s;
    }
  }

  {
    auto &upstreamconf = connconf.upstream;
    {
      auto &timeoutconf = upstreamconf.timeout;
      // Idle timeout for HTTP2 upstream connection
      timeoutconf.http2_idle = 3_min;

      // Idle timeout for HTTP3 upstream connection
      timeoutconf.http3_idle = 3_min;

      // Write timeout for HTTP2/non-HTTP2 upstream connection
      timeoutconf.write = 30_s;

      // Keep alive (idle) timeout for HTTP/1 upstream connection
      timeoutconf.idle = 1_min;
    }
  }

  {
    connconf.downstream = std::make_shared<DownstreamConfig>();
    auto &downstreamconf = *connconf.downstream;
    {
      auto &timeoutconf = downstreamconf.timeout;
      // Read/Write timeouts for downstream connection
      timeoutconf.read = 1_min;
      timeoutconf.write = 30_s;
      // Timeout for pooled (idle) connections
      timeoutconf.idle_read = 2_s;
      timeoutconf.connect = 30_s;
      timeoutconf.max_backoff = 120_s;
    }

    downstreamconf.connections_per_host = 8;
    downstreamconf.request_buffer_size = 16_k;
    downstreamconf.response_buffer_size = 128_k;
    downstreamconf.family = AF_UNSPEC;
  }

  auto &apiconf = config->api;
  apiconf.max_request_body = 32_m;

  auto &dnsconf = config->dns;
  {
    auto &timeoutconf = dnsconf.timeout;
    timeoutconf.cache = 10_s;
    timeoutconf.lookup = 5_s;
  }
  dnsconf.max_try = 2;
}

} // namespace

namespace {
void print_version(std::ostream &out) {
  out << "nghttpx nghttp2/" NGHTTP2_VERSION
#ifdef ENABLE_HTTP3
         " ngtcp2/" NGTCP2_VERSION " nghttp3/" NGHTTP3_VERSION
#endif // ENABLE_HTTP3
      << std::endl;
}
} // namespace

namespace {
void print_usage(std::ostream &out) {
  out << R"(Usage: nghttpx [OPTIONS]... [<PRIVATE_KEY> <CERT>]
A reverse proxy for HTTP/3, HTTP/2, and HTTP/1.)"
      << std::endl;
}
} // namespace

namespace {
void print_help(std::ostream &out) {
  auto config = get_config();

  print_usage(out);
  out << R"(
  <PRIVATE_KEY>
              Set  path  to  server's private  key.   Required  unless
              "no-tls" parameter is used in --frontend option.
  <CERT>      Set  path  to  server's  certificate.   Required  unless
              "no-tls"  parameter is  used in  --frontend option.   To
              make OCSP stapling work, this must be an absolute path.

Options:
  The options are categorized into several groups.

Connections:
  -b, --backend=(<HOST>,<PORT>|unix:<PATH>)[;[<PATTERN>[:...]][[;<PARAM>]...]

              Set  backend  host  and   port.   The  multiple  backend
              addresses are  accepted by repeating this  option.  UNIX
              domain socket  can be  specified by prefixing  path name
              with "unix:" (e.g., unix:/var/run/backend.sock).

              Optionally, if <PATTERN>s are given, the backend address
              is  only  used  if  request matches  the  pattern.   The
              pattern  matching is  closely  designed  to ServeMux  in
              net/http package of  Go programming language.  <PATTERN>
              consists of  path, host +  path or just host.   The path
              must start  with "/".  If  it ends with "/",  it matches
              all  request path  in  its subtree.   To  deal with  the
              request  to the  directory without  trailing slash,  the
              path which ends  with "/" also matches  the request path
              which  only  lacks  trailing  '/'  (e.g.,  path  "/foo/"
              matches request path  "/foo").  If it does  not end with
              "/", it  performs exact match against  the request path.
              If  host  is given,  it  performs  a match  against  the
              request host.   For a  request received on  the frontend
              listener with  "sni-fwd" parameter enabled, SNI  host is
              used instead of a request host.  If host alone is given,
              "/" is  appended to it,  so that it matches  all request
              paths  under the  host  (e.g., specifying  "nghttp2.org"
              equals  to "nghttp2.org/").   CONNECT method  is treated
              specially.  It  does not have  path, and we  don't allow
              empty path.  To workaround  this, we assume that CONNECT
              method has "/" as path.

              Patterns with  host take  precedence over  patterns with
              just path.   Then, longer patterns take  precedence over
              shorter ones.

              Host  can  include "*"  in  the  left most  position  to
              indicate  wildcard match  (only suffix  match is  done).
              The "*" must match at least one character.  For example,
              host    pattern    "*.nghttp2.org"    matches    against
              "www.nghttp2.org"  and  "git.ngttp2.org", but  does  not
              match  against  "nghttp2.org".   The exact  hosts  match
              takes precedence over the wildcard hosts match.

              If path  part ends with  "*", it is treated  as wildcard
              path.  The  wildcard path  behaves differently  from the
              normal path.  For normal path,  match is made around the
              boundary of path component  separator,"/".  On the other
              hand, the wildcard  path does not take  into account the
              path component  separator.  All paths which  include the
              wildcard  path  without  last  "*" as  prefix,  and  are
              strictly longer than wildcard  path without last "*" are
              matched.  "*"  must match  at least one  character.  For
              example,  the   pattern  "/foo*"  matches   "/foo/"  and
              "/foobar".  But it does not match "/foo", or "/fo".

              If <PATTERN> is omitted or  empty string, "/" is used as
              pattern,  which  matches  all request  paths  (catch-all
              pattern).  The catch-all backend must be given.

              When doing  a match, nghttpx made  some normalization to
              pattern, request host and path.  For host part, they are
              converted to lower case.  For path part, percent-encoded
              unreserved characters  defined in RFC 3986  are decoded,
              and any  dot-segments (".."  and ".")   are resolved and
              removed.

              For   example,   -b'127.0.0.1,8080;nghttp2.org/httpbin/'
              matches the  request host "nghttp2.org" and  the request
              path "/httpbin/get", but does not match the request host
              "nghttp2.org" and the request path "/index.html".

              The  multiple <PATTERN>s  can  be specified,  delimiting
              them            by           ":".             Specifying
              -b'127.0.0.1,8080;nghttp2.org:www.nghttp2.org'  has  the
              same  effect  to specify  -b'127.0.0.1,8080;nghttp2.org'
              and -b'127.0.0.1,8080;www.nghttp2.org'.

              The backend addresses sharing same <PATTERN> are grouped
              together forming  load balancing  group.

              Several parameters <PARAM> are accepted after <PATTERN>.
              The  parameters are  delimited  by  ";".  The  available
              parameters       are:      "proto=<PROTO>",       "tls",
              "sni=<SNI_HOST>",         "fall=<N>",        "rise=<N>",
              "affinity=<METHOD>",    "dns",    "redirect-if-not-tls",
              "upgrade-scheme",                        "mruby=<PATH>",
              "read-timeout=<DURATION>",   "write-timeout=<DURATION>",
              "group=<GROUP>",  "group-weight=<N>", "weight=<N>",  and
              "dnf".    The  parameter   consists   of  keyword,   and
              optionally followed by "="  and value.  For example, the
              parameter "proto=h2" consists of the keyword "proto" and
              value "h2".  The parameter "tls" consists of the keyword
              "tls"  without value.   Each parameter  is described  as
              follows.

              The backend application protocol  can be specified using
              optional  "proto"   parameter,  and   in  the   form  of
              "proto=<PROTO>".  <PROTO> should be one of the following
              list  without  quotes:  "h2", "http/1.1".   The  default
              value of <PROTO> is  "http/1.1".  Note that usually "h2"
              refers to HTTP/2  over TLS.  But in this  option, it may
              mean HTTP/2  over cleartext TCP unless  "tls" keyword is
              used (see below).

              TLS  can   be  enabled  by  specifying   optional  "tls"
              parameter.  TLS is not enabled by default.

              With "sni=<SNI_HOST>" parameter, it can override the TLS
              SNI  field  value  with  given  <SNI_HOST>.   This  will
              default to the backend <HOST> name

              The  feature  to detect  whether  backend  is online  or
              offline can be enabled  using optional "fall" and "rise"
              parameters.   Using  "fall=<N>"  parameter,  if  nghttpx
              cannot connect  to a  this backend <N>  times in  a row,
              this  backend  is  assumed  to be  offline,  and  it  is
              excluded from load balancing.  If <N> is 0, this backend
              never  be excluded  from load  balancing whatever  times
              nghttpx cannot connect  to it, and this  is the default.
              There is  also "rise=<N>" parameter.  After  backend was
              excluded from load balancing group, nghttpx periodically
              attempts to make a connection to the failed backend, and
              if the  connection is made  successfully <N> times  in a
              row, the backend is assumed to  be online, and it is now
              eligible  for load  balancing target.   If <N>  is 0,  a
              backend  is permanently  offline, once  it goes  in that
              state, and this is the default behaviour.

              The     session     affinity    is     enabled     using
              "affinity=<METHOD>"  parameter.   If  "ip" is  given  in
              <METHOD>, client  IP based session affinity  is enabled.
              If "cookie"  is given in <METHOD>,  cookie based session
              affinity is  enabled.  If  "none" is given  in <METHOD>,
              session affinity  is disabled, and this  is the default.
              The session  affinity is  enabled per <PATTERN>.   If at
              least  one backend  has  "affinity"  parameter, and  its
              <METHOD> is not "none",  session affinity is enabled for
              all backend  servers sharing the same  <PATTERN>.  It is
              advised  to  set  "affinity" parameter  to  all  backend
              explicitly if session affinity  is desired.  The session
              affinity  may   break  if   one  of  the   backend  gets
              unreachable,  or   backend  settings  are   reloaded  or
              replaced by API.

              If   "affinity=cookie"    is   used,    the   additional
              configuration                is                required.
              "affinity-cookie-name=<NAME>" must be  used to specify a
              name     of     cookie      to     use.      Optionally,
              "affinity-cookie-path=<PATH>" can  be used to  specify a
              path   which   cookie    is   applied.    The   optional
              "affinity-cookie-secure=<SECURE>"  controls  the  Secure
              attribute of a cookie.  The default value is "auto", and
              the Secure attribute is  determined by a request scheme.
              If a request scheme is "https", then Secure attribute is
              set.  Otherwise, it  is not set.  If  <SECURE> is "yes",
              the  Secure attribute  is  always set.   If <SECURE>  is
              "no",   the   Secure   attribute  is   always   omitted.
              "affinity-cookie-stickiness=<STICKINESS>"       controls
              stickiness  of   this  affinity.   If   <STICKINESS>  is
              "loose", removing or adding a backend server might break
              the affinity  and the  request might  be forwarded  to a
              different backend server.   If <STICKINESS> is "strict",
              removing the designated  backend server breaks affinity,
              but adding  new backend server does  not cause breakage.
              If  the designated  backend server  becomes unavailable,
              new backend server is chosen  as if the request does not
              have  an  affinity  cookie.   <STICKINESS>  defaults  to
              "loose".

              By default, name resolution of backend host name is done
              at  start  up,  or reloading  configuration.   If  "dns"
              parameter   is  given,   name  resolution   takes  place
              dynamically.  This is useful  if backend address changes
              frequently.   If  "dns"  is given,  name  resolution  of
              backend   host   name   at  start   up,   or   reloading
              configuration is skipped.

              If "redirect-if-not-tls" parameter  is used, the matched
              backend  requires   that  frontend  connection   is  TLS
              encrypted.  If it isn't, nghttpx responds to the request
              with 308  status code, and  https URI the  client should
              use instead  is included in Location  header field.  The
              port number in  redirect URI is 443 by  default, and can
              be  changed using  --redirect-https-port option.   If at
              least one  backend has  "redirect-if-not-tls" parameter,
              this feature is enabled  for all backend servers sharing
              the   same   <PATTERN>.    It    is   advised   to   set
              "redirect-if-no-tls"    parameter   to    all   backends
              explicitly if this feature is desired.

              If "upgrade-scheme"  parameter is used along  with "tls"
              parameter, HTTP/2 :scheme pseudo header field is changed
              to "https" from "http" when forwarding a request to this
              particular backend.  This is  a workaround for a backend
              server  which  requires  "https" :scheme  pseudo  header
              field on TLS encrypted connection.

              "mruby=<PATH>"  parameter  specifies  a  path  to  mruby
              script  file  which  is  invoked when  this  pattern  is
              matched.  All backends which share the same pattern must
              have the same mruby path.

              "read-timeout=<DURATION>" and "write-timeout=<DURATION>"
              parameters  specify the  read and  write timeout  of the
              backend connection  when this  pattern is  matched.  All
              backends which share the same pattern must have the same
              timeouts.  If these timeouts  are entirely omitted for a
              pattern,            --backend-read-timeout           and
              --backend-write-timeout are used.

              "group=<GROUP>"  parameter specifies  the name  of group
              this backend address belongs to.  By default, it belongs
              to  the unnamed  default group.   The name  of group  is
              unique   per   pattern.   "group-weight=<N>"   parameter
              specifies the  weight of  the group.  The  higher weight
              gets  more frequently  selected  by  the load  balancing
              algorithm.  <N> must be  [1, 256] inclusive.  The weight
              8 has 4 times more weight  than 2.  <N> must be the same
              for  all addresses  which  share the  same <GROUP>.   If
              "group-weight" is  omitted in an address,  but the other
              address  which  belongs  to  the  same  group  specifies
              "group-weight",   its    weight   is   used.     If   no
              "group-weight"  is  specified  for  all  addresses,  the
              weight of a group becomes 1.  "group" and "group-weight"
              are ignored if session affinity is enabled.

              "weight=<N>"  parameter  specifies  the  weight  of  the
              backend  address  inside  a  group  which  this  address
              belongs  to.  The  higher  weight  gets more  frequently
              selected by  the load balancing algorithm.   <N> must be
              [1,  256] inclusive.   The  weight 8  has  4 times  more
              weight  than weight  2.  If  this parameter  is omitted,
              weight  becomes  1.   "weight"  is  ignored  if  session
              affinity is enabled.

              If "dnf" parameter is  specified, an incoming request is
              not forwarded to a backend  and just consumed along with
              the  request body  (actually a  backend server  never be
              contacted).  It  is expected  that the HTTP  response is
              generated by mruby  script (see "mruby=<PATH>" parameter
              above).  "dnf" is an abbreviation of "do not forward".

              Since ";" and ":" are  used as delimiter, <PATTERN> must
              not contain  these characters.  In order  to include ":"
              in  <PATTERN>,  one  has  to  specify  "%3A"  (which  is
              percent-encoded  from of  ":") instead.   Since ";"  has
              special  meaning  in shell,  the  option  value must  be
              quoted.

              Default: )"
      << DEFAULT_DOWNSTREAM_HOST << "," << DEFAULT_DOWNSTREAM_PORT << R"(
  -f, --frontend=(<HOST>,<PORT>|unix:<PATH>)[[;<PARAM>]...]
              Set  frontend  host and  port.   If  <HOST> is  '*',  it
              assumes  all addresses  including  both  IPv4 and  IPv6.
              UNIX domain  socket can  be specified by  prefixing path
              name  with  "unix:" (e.g.,  unix:/var/run/nghttpx.sock).
              This  option can  be used  multiple times  to listen  to
              multiple addresses.

              This option  can take  0 or  more parameters,  which are
              described  below.   Note   that  "api"  and  "healthmon"
              parameters are mutually exclusive.

              Optionally, TLS  can be disabled by  specifying "no-tls"
              parameter.  TLS is enabled by default.

              If "sni-fwd" parameter is  used, when performing a match
              to select a backend server,  SNI host name received from
              the client  is used  instead of  the request  host.  See
              --backend option about the pattern match.

              To  make this  frontend as  API endpoint,  specify "api"
              parameter.   This   is  disabled  by  default.    It  is
              important  to  limit the  access  to  the API  frontend.
              Otherwise, someone  may change  the backend  server, and
              break your services,  or expose confidential information
              to the outside the world.

              To  make  this  frontend  as  health  monitor  endpoint,
              specify  "healthmon"  parameter.   This is  disabled  by
              default.  Any  requests which come through  this address
              are replied with 200 HTTP status, without no body.

              To accept  PROXY protocol  version 1  and 2  on frontend
              connection,  specify  "proxyproto" parameter.   This  is
              disabled by default.

              To  receive   HTTP/3  (QUIC)  traffic,   specify  "quic"
              parameter.  It  makes nghttpx listen on  UDP port rather
              than  TCP   port.   UNIX   domain  socket,   "api",  and
              "healthmon"  parameters  cannot   be  used  with  "quic"
              parameter.

              Default: *,3000
  --backlog=<N>
              Set listen backlog size.
              Default: )"
      << config->conn.listener.backlog << R"(
  --backend-address-family=(auto|IPv4|IPv6)
              Specify  address  family  of  backend  connections.   If
              "auto" is given, both IPv4  and IPv6 are considered.  If
              "IPv4" is  given, only  IPv4 address is  considered.  If
              "IPv6" is given, only IPv6 address is considered.
              Default: auto
  --backend-http-proxy-uri=<URI>
              Specify      proxy       URI      in       the      form
              http://[<USER>:<PASS>@]<PROXY>:<PORT>.    If   a   proxy
              requires  authentication,  specify  <USER>  and  <PASS>.
              Note that  they must be properly  percent-encoded.  This
              proxy  is used  when the  backend connection  is HTTP/2.
              First,  make  a CONNECT  request  to  the proxy  and  it
              connects  to the  backend  on behalf  of nghttpx.   This
              forms  tunnel.   After  that, nghttpx  performs  SSL/TLS
              handshake with  the downstream through the  tunnel.  The
              timeouts when connecting and  making CONNECT request can
              be     specified    by     --backend-read-timeout    and
              --backend-write-timeout options.

Performance:
  -n, --workers=<N>
              Set the number of worker threads.
              Default: )"
      << config->num_worker << R"(
  --single-thread
              Run everything in one  thread inside the worker process.
              This   feature   is   provided  for   better   debugging
              experience,  or  for  the platforms  which  lack  thread
              support.   If  threading  is disabled,  this  option  is
              always enabled.
  --read-rate=<SIZE>
              Set maximum  average read  rate on  frontend connection.
              Setting 0 to this option means read rate is unlimited.
              Default: )"
      << config->conn.upstream.ratelimit.read.rate << R"(
  --read-burst=<SIZE>
              Set  maximum read  burst  size  on frontend  connection.
              Setting  0  to this  option  means  read burst  size  is
              unlimited.
              Default: )"
      << config->conn.upstream.ratelimit.read.burst << R"(
  --write-rate=<SIZE>
              Set maximum  average write rate on  frontend connection.
              Setting 0 to this option means write rate is unlimited.
              Default: )"
      << config->conn.upstream.ratelimit.write.rate << R"(
  --write-burst=<SIZE>
              Set  maximum write  burst size  on frontend  connection.
              Setting  0 to  this  option means  write  burst size  is
              unlimited.
              Default: )"
      << config->conn.upstream.ratelimit.write.burst << R"(
  --worker-read-rate=<SIZE>
              Set maximum average read rate on frontend connection per
              worker.  Setting  0 to  this option  means read  rate is
              unlimited.  Not implemented yet.
              Default: 0
  --worker-read-burst=<SIZE>
              Set maximum  read burst size on  frontend connection per
              worker.  Setting 0 to this  option means read burst size
              is unlimited.  Not implemented yet.
              Default: 0
  --worker-write-rate=<SIZE>
              Set maximum  average write  rate on  frontend connection
              per worker.  Setting  0 to this option  means write rate
              is unlimited.  Not implemented yet.
              Default: 0
  --worker-write-burst=<SIZE>
              Set maximum write burst  size on frontend connection per
              worker.  Setting 0 to this option means write burst size
              is unlimited.  Not implemented yet.
              Default: 0
  --worker-frontend-connections=<N>
              Set maximum number  of simultaneous connections frontend
              accepts.  Setting 0 means unlimited.
              Default: )"
      << config->conn.upstream.worker_connections << R"(
  --backend-connections-per-host=<N>
              Set  maximum number  of  backend concurrent  connections
              (and/or  streams in  case  of HTTP/2)  per origin  host.
              This option  is meaningful when --http2-proxy  option is
              used.   The  origin  host  is  determined  by  authority
              portion of  request URI (or :authority  header field for
              HTTP/2).   To  limit  the   number  of  connections  per
              frontend        for       default        mode,       use
              --backend-connections-per-frontend.
              Default: )"
      << config->conn.downstream->connections_per_host << R"(
  --backend-connections-per-frontend=<N>
              Set  maximum number  of  backend concurrent  connections
              (and/or streams  in case of HTTP/2)  per frontend.  This
              option  is   only  used  for  default   mode.   0  means
              unlimited.  To limit the  number of connections per host
              with          --http2-proxy         option,          use
              --backend-connections-per-host.
              Default: )"
      << config->conn.downstream->connections_per_frontend << R"(
  --rlimit-nofile=<N>
              Set maximum number of open files (RLIMIT_NOFILE) to <N>.
              If 0 is given, nghttpx does not set the limit.
              Default: )"
      << config->rlimit_nofile << R"(
  --rlimit-memlock=<N>
              Set maximum number of bytes of memory that may be locked
              into  RAM.  If  0 is  given,  nghttpx does  not set  the
              limit.
              Default: )"
      << config->rlimit_memlock << R"(
  --backend-request-buffer=<SIZE>
              Set buffer size used to store backend request.
              Default: )"
      << util::utos_unit(config->conn.downstream->request_buffer_size) << R"(
  --backend-response-buffer=<SIZE>
              Set buffer size used to store backend response.
              Default: )"
      << util::utos_unit(config->conn.downstream->response_buffer_size) << R"(
  --fastopen=<N>
              Enables  "TCP Fast  Open" for  the listening  socket and
              limits the  maximum length for the  queue of connections
              that have not yet completed the three-way handshake.  If
              value is 0 then fast open is disabled.
              Default: )"
      << config->conn.listener.fastopen << R"(
  --no-kqueue Don't use  kqueue.  This  option is only  applicable for
              the platforms  which have kqueue.  For  other platforms,
              this option will be simply ignored.

Timeout:
  --frontend-http2-idle-timeout=<DURATION>
              Specify idle timeout for HTTP/2 frontend connection.  If
              no active streams exist for this duration, connection is
              closed.
              Default: )"
      << util::duration_str(config->conn.upstream.timeout.http2_idle) << R"(
  --frontend-http3-idle-timeout=<DURATION>
              Specify idle timeout for HTTP/3 frontend connection.  If
              no active streams exist for this duration, connection is
              closed.
              Default: )"
      << util::duration_str(config->conn.upstream.timeout.http3_idle) << R"(
  --frontend-write-timeout=<DURATION>
              Specify write timeout for all frontend connections.
              Default: )"
      << util::duration_str(config->conn.upstream.timeout.write) << R"(
  --frontend-keep-alive-timeout=<DURATION>
              Specify   keep-alive   timeout   for   frontend   HTTP/1
              connection.
              Default: )"
      << util::duration_str(config->conn.upstream.timeout.idle) << R"(
  --frontend-header-timeout=<DURATION>
              Specify  duration  that the  server  waits  for an  HTTP
              request  header fields  to be  received completely.   On
              timeout, HTTP/1 and HTTP/2  connections are closed.  For
              HTTP/3,  the  stream  is shutdown,  and  the  connection
              itself is left intact.
              Default: )"
      << util::duration_str(config->http.timeout.header) << R"(
  --stream-read-timeout=<DURATION>
              Specify  read timeout  for HTTP/2  streams.  0  means no
              timeout.
              Default: )"
      << util::duration_str(config->http2.timeout.stream_read) << R"(
  --stream-write-timeout=<DURATION>
              Specify write  timeout for  HTTP/2 streams.  0  means no
              timeout.
              Default: )"
      << util::duration_str(config->http2.timeout.stream_write) << R"(
  --backend-read-timeout=<DURATION>
              Specify read timeout for backend connection.
              Default: )"
      << util::duration_str(config->conn.downstream->timeout.read) << R"(
  --backend-write-timeout=<DURATION>
              Specify write timeout for backend connection.
              Default: )"
      << util::duration_str(config->conn.downstream->timeout.write) << R"(
  --backend-connect-timeout=<DURATION>
              Specify  timeout before  establishing TCP  connection to
              backend.
              Default: )"
      << util::duration_str(config->conn.downstream->timeout.connect) << R"(
  --backend-keep-alive-timeout=<DURATION>
              Specify   keep-alive   timeout    for   backend   HTTP/1
              connection.
              Default: )"
      << util::duration_str(config->conn.downstream->timeout.idle_read) << R"(
  --listener-disable-timeout=<DURATION>
              After accepting  connection failed,  connection listener
              is disabled  for a given  amount of time.   Specifying 0
              disables this feature.
              Default: )"
      << util::duration_str(config->conn.listener.timeout.sleep) << R"(
  --frontend-http2-setting-timeout=<DURATION>
              Specify  timeout before  SETTINGS ACK  is received  from
              client.
              Default: )"
      << util::duration_str(config->http2.upstream.timeout.settings) << R"(
  --backend-http2-settings-timeout=<DURATION>
              Specify  timeout before  SETTINGS ACK  is received  from
              backend server.
              Default: )"
      << util::duration_str(config->http2.downstream.timeout.settings) << R"(
  --backend-max-backoff=<DURATION>
              Specify  maximum backoff  interval.  This  is used  when
              doing health  check against offline backend  (see "fail"
              parameter  in --backend  option).   It is  also used  to
              limit  the  maximum   interval  to  temporarily  disable
              backend  when nghttpx  failed to  connect to  it.  These
              intervals are calculated  using exponential backoff, and
              consecutive failed attempts increase the interval.  This
              option caps its maximum value.
              Default: )"
      << util::duration_str(config->conn.downstream->timeout.max_backoff) << R"(

SSL/TLS:
  --ciphers=<SUITE>
              Set allowed  cipher list  for frontend  connection.  The
              format of the string is described in OpenSSL ciphers(1).
              This option  sets cipher suites for  TLSv1.2 or earlier.
              Use --tls13-ciphers for TLSv1.3.
              Default: )"
      << config->tls.ciphers << R"(
  --tls13-ciphers=<SUITE>
              Set allowed  cipher list  for frontend  connection.  The
              format of the string is described in OpenSSL ciphers(1).
              This  option  sets  cipher   suites  for  TLSv1.3.   Use
              --ciphers for TLSv1.2 or earlier.
              Default: )"
      << config->tls.tls13_ciphers << R"(
  --client-ciphers=<SUITE>
              Set  allowed cipher  list for  backend connection.   The
              format of the string is described in OpenSSL ciphers(1).
              This option  sets cipher suites for  TLSv1.2 or earlier.
              Use --tls13-client-ciphers for TLSv1.3.
              Default: )"
      << config->tls.client.ciphers << R"(
  --tls13-client-ciphers=<SUITE>
              Set  allowed cipher  list for  backend connection.   The
              format of the string is described in OpenSSL ciphers(1).
              This  option  sets  cipher   suites  for  TLSv1.3.   Use
              --tls13-client-ciphers for TLSv1.2 or earlier.
              Default: )"
      << config->tls.client.tls13_ciphers << R"(
  --ecdh-curves=<LIST>
              Set  supported  curve  list  for  frontend  connections.
              <LIST> is a  colon separated list of curve  NID or names
              in the preference order.  The supported curves depend on
              the  linked  OpenSSL  library.  This  function  requires
              OpenSSL >= 1.0.2.
              Default: )"
      << config->tls.ecdh_curves << R"(
  -k, --insecure
              Don't  verify backend  server's  certificate  if TLS  is
              enabled for backend connections.
  --cacert=<PATH>
              Set path to trusted CA  certificate file.  It is used in
              backend  TLS connections  to verify  peer's certificate.
              It is also used to  verify OCSP response from the script
              set by --fetch-ocsp-response-file.  The  file must be in
              PEM format.   It can contain multiple  certificates.  If
              the  linked OpenSSL  is configured  to load  system wide
              certificates, they  are loaded at startup  regardless of
              this option.
  --private-key-passwd-file=<PATH>
              Path  to file  that contains  password for  the server's
              private key.   If none is  given and the private  key is
              password protected it'll be requested interactively.
  --subcert=<KEYPATH>:<CERTPATH>[[;<PARAM>]...]
              Specify  additional certificate  and  private key  file.
              nghttpx will  choose certificates based on  the hostname
              indicated by client using TLS SNI extension.  If nghttpx
              is  built with  OpenSSL  >= 1.0.2,  the shared  elliptic
              curves (e.g., P-256) between  client and server are also
              taken into  consideration.  This allows nghttpx  to send
              ECDSA certificate  to modern clients, while  sending RSA
              based certificate to older  clients.  This option can be
              used  multiple  times.   To  make  OCSP  stapling  work,
              <CERTPATH> must be absolute path.

              Additional parameter  can be specified in  <PARAM>.  The
              available <PARAM> is "sct-dir=<DIR>".

              "sct-dir=<DIR>"  specifies the  path to  directory which
              contains        *.sct        files        for        TLS
              signed_certificate_timestamp extension (RFC 6962).  This
              feature   requires   OpenSSL   >=   1.0.2.    See   also
              --tls-sct-dir option.
  --dh-param-file=<PATH>
              Path to file that contains  DH parameters in PEM format.
              Without  this   option,  DHE   cipher  suites   are  not
              available.
  --alpn-list=<LIST>
              Comma delimited list of  ALPN protocol identifier sorted
              in the  order of preference.  That  means most desirable
              protocol comes  first.  The parameter must  be delimited
              by a single comma only  and any white spaces are treated
              as a part of protocol string.
              Default: )"
      << DEFAULT_ALPN_LIST
      << R"(
  --verify-client
              Require and verify client certificate.
  --verify-client-cacert=<PATH>
              Path  to file  that contains  CA certificates  to verify
              client certificate.  The file must be in PEM format.  It
              can contain multiple certificates.
  --verify-client-tolerate-expired
              Accept  expired  client  certificate.   Operator  should
              handle  the expired  client  certificate  by some  means
              (e.g.,  mruby  script).   Otherwise, this  option  might
              cause a security risk.
  --client-private-key-file=<PATH>
              Path to  file that contains  client private key  used in
              backend client authentication.
  --client-cert-file=<PATH>
              Path to  file that  contains client certificate  used in
              backend client authentication.
  --tls-min-proto-version=<VER>
              Specify minimum SSL/TLS protocol.   The name matching is
              done in  case-insensitive manner.  The  versions between
              --tls-min-proto-version and  --tls-max-proto-version are
              enabled.  If the protocol list advertised by client does
              not  overlap  this range,  you  will  receive the  error
              message "unknown protocol".  If a protocol version lower
              than TLSv1.2 is specified, make sure that the compatible
              ciphers are  included in --ciphers option.   The default
              cipher  list  only   includes  ciphers  compatible  with
              TLSv1.2 or above.  The available versions are:
              )"
#ifdef TLS1_3_VERSION
         "TLSv1.3, "
#endif // TLS1_3_VERSION
         "TLSv1.2, TLSv1.1, and TLSv1.0"
         R"(
              Default: )"
      << DEFAULT_TLS_MIN_PROTO_VERSION
      << R"(
  --tls-max-proto-version=<VER>
              Specify maximum SSL/TLS protocol.   The name matching is
              done in  case-insensitive manner.  The  versions between
              --tls-min-proto-version and  --tls-max-proto-version are
              enabled.  If the protocol list advertised by client does
              not  overlap  this range,  you  will  receive the  error
              message "unknown protocol".  The available versions are:
              )"
#ifdef TLS1_3_VERSION
         "TLSv1.3, "
#endif // TLS1_3_VERSION
         "TLSv1.2, TLSv1.1, and TLSv1.0"
         R"(
              Default: )"
      << DEFAULT_TLS_MAX_PROTO_VERSION << R"(
  --tls-ticket-key-file=<PATH>
              Path to file that contains  random data to construct TLS
              session ticket  parameters.  If aes-128-cbc is  given in
              --tls-ticket-key-cipher, the  file must  contain exactly
              48    bytes.     If     aes-256-cbc    is    given    in
              --tls-ticket-key-cipher, the  file must  contain exactly
              80  bytes.   This  options  can be  used  repeatedly  to
              specify  multiple ticket  parameters.  If  several files
              are given,  only the  first key is  used to  encrypt TLS
              session  tickets.  Other  keys are  accepted but  server
              will  issue new  session  ticket with  first key.   This
              allows  session  key  rotation.  Please  note  that  key
              rotation  does  not  occur automatically.   User  should
              rearrange  files or  change options  values and  restart
              nghttpx gracefully.   If opening  or reading  given file
              fails, all loaded  keys are discarded and  it is treated
              as if none  of this option is given.  If  this option is
              not given or an error  occurred while opening or reading
              a file,  key is  generated every  1 hour  internally and
              they are  valid for  12 hours.   This is  recommended if
              ticket  key sharing  between  nghttpx  instances is  not
              required.
  --tls-ticket-key-memcached=<HOST>,<PORT>[;tls]
              Specify address  of memcached  server to get  TLS ticket
              keys for  session resumption.   This enables  shared TLS
              ticket key between  multiple nghttpx instances.  nghttpx
              does not set TLS ticket  key to memcached.  The external
              ticket key generator is required.  nghttpx just gets TLS
              ticket  keys  from  memcached, and  use  them,  possibly
              replacing current set  of keys.  It is up  to extern TLS
              ticket  key generator  to rotate  keys frequently.   See
              "TLS SESSION  TICKET RESUMPTION" section in  manual page
              to know the data format in memcached entry.  Optionally,
              memcached  connection  can  be  encrypted  with  TLS  by
              specifying "tls" parameter.
  --tls-ticket-key-memcached-address-family=(auto|IPv4|IPv6)
              Specify address  family of memcached connections  to get
              TLS ticket keys.  If "auto" is given, both IPv4 and IPv6
              are considered.   If "IPv4" is given,  only IPv4 address
              is considered.  If "IPv6" is given, only IPv6 address is
              considered.
              Default: auto
  --tls-ticket-key-memcached-interval=<DURATION>
              Set interval to get TLS ticket keys from memcached.
              Default: )"
      << util::duration_str(config->tls.ticket.memcached.interval) << R"(
  --tls-ticket-key-memcached-max-retry=<N>
              Set  maximum   number  of  consecutive   retries  before
              abandoning TLS ticket key  retrieval.  If this number is
              reached,  the  attempt  is considered  as  failure,  and
              "failure" count  is incremented by 1,  which contributed
              to            the            value            controlled
              --tls-ticket-key-memcached-max-fail option.
              Default: )"
      << config->tls.ticket.memcached.max_retry << R"(
  --tls-ticket-key-memcached-max-fail=<N>
              Set  maximum   number  of  consecutive   failure  before
              disabling TLS ticket until next scheduled key retrieval.
              Default: )"
      << config->tls.ticket.memcached.max_fail << R"(
  --tls-ticket-key-cipher=<CIPHER>
              Specify cipher  to encrypt TLS session  ticket.  Specify
              either   aes-128-cbc   or  aes-256-cbc.    By   default,
              aes-128-cbc is used.
  --tls-ticket-key-memcached-cert-file=<PATH>
              Path to client certificate  for memcached connections to
              get TLS ticket keys.
  --tls-ticket-key-memcached-private-key-file=<PATH>
              Path to client private  key for memcached connections to
              get TLS ticket keys.
  --fetch-ocsp-response-file=<PATH>
              Path to  fetch-ocsp-response script file.  It  should be
              absolute path.
              Default: )"
      << config->tls.ocsp.fetch_ocsp_response_file << R"(
  --ocsp-update-interval=<DURATION>
              Set interval to update OCSP response cache.
              Default: )"
      << util::duration_str(config->tls.ocsp.update_interval) << R"(
  --ocsp-startup
              Start  accepting connections  after initial  attempts to
              get OCSP responses  finish.  It does not  matter some of
              the  attempts  fail.  This  feature  is  useful if  OCSP
              responses   must    be   available    before   accepting
              connections.
  --no-verify-ocsp
              nghttpx does not verify OCSP response.
  --no-ocsp   Disable OCSP stapling.
  --tls-session-cache-memcached=<HOST>,<PORT>[;tls]
              Specify  address of  memcached server  to store  session
              cache.   This  enables   shared  session  cache  between
              multiple   nghttpx  instances.    Optionally,  memcached
              connection can be encrypted with TLS by specifying "tls"
              parameter.
  --tls-session-cache-memcached-address-family=(auto|IPv4|IPv6)
              Specify address family of memcached connections to store
              session cache.  If  "auto" is given, both  IPv4 and IPv6
              are considered.   If "IPv4" is given,  only IPv4 address
              is considered.  If "IPv6" is given, only IPv6 address is
              considered.
              Default: auto
  --tls-session-cache-memcached-cert-file=<PATH>
              Path to client certificate  for memcached connections to
              store session cache.
  --tls-session-cache-memcached-private-key-file=<PATH>
              Path to client private  key for memcached connections to
              store session cache.
  --tls-dyn-rec-warmup-threshold=<SIZE>
              Specify the  threshold size for TLS  dynamic record size
              behaviour.  During  a TLS  session, after  the threshold
              number of bytes  have been written, the  TLS record size
              will be increased to the maximum allowed (16K).  The max
              record size will  continue to be used on  the active TLS
              session.  After  --tls-dyn-rec-idle-timeout has elapsed,
              the record size is reduced  to 1300 bytes.  Specify 0 to
              always use  the maximum record size,  regardless of idle
              period.   This  behaviour  applies   to  all  TLS  based
              frontends, and TLS HTTP/2 backends.
              Default: )"
      << util::utos_unit(config->tls.dyn_rec.warmup_threshold) << R"(
  --tls-dyn-rec-idle-timeout=<DURATION>
              Specify TLS dynamic record  size behaviour timeout.  See
              --tls-dyn-rec-warmup-threshold  for   more  information.
              This behaviour  applies to all TLS  based frontends, and
              TLS HTTP/2 backends.
              Default: )"
      << util::duration_str(config->tls.dyn_rec.idle_timeout) << R"(
  --no-http2-cipher-block-list
              Allow  block  listed  cipher suite  on  frontend  HTTP/2
              connection.                                          See
              https://tools.ietf.org/html/rfc7540#appendix-A  for  the
              complete HTTP/2 cipher suites block list.
  --client-no-http2-cipher-block-list
              Allow  block  listed  cipher  suite  on  backend  HTTP/2
              connection.                                          See
              https://tools.ietf.org/html/rfc7540#appendix-A  for  the
              complete HTTP/2 cipher suites block list.
  --tls-sct-dir=<DIR>
              Specifies the  directory where  *.sct files  exist.  All
              *.sct   files   in  <DIR>   are   read,   and  sent   as
              extension_data of  TLS signed_certificate_timestamp (RFC
              6962)  to  client.   These   *.sct  files  are  for  the
              certificate   specified   in   positional   command-line
              argument <CERT>, or  certificate option in configuration
              file.   For   additional  certificates,   use  --subcert
              option.  This option requires OpenSSL >= 1.0.2.
  --psk-secrets=<PATH>
              Read list of PSK identity and secrets from <PATH>.  This
              is used for frontend connection.  The each line of input
              file  is  formatted  as  <identity>:<hex-secret>,  where
              <identity> is  PSK identity, and <hex-secret>  is secret
              in hex.  An  empty line, and line which  starts with '#'
              are skipped.  The default  enabled cipher list might not
              contain any PSK cipher suite.  In that case, desired PSK
              cipher suites  must be  enabled using  --ciphers option.
              The  desired PSK  cipher suite  may be  block listed  by
              HTTP/2.   To  use  those   cipher  suites  with  HTTP/2,
              consider  to  use  --no-http2-cipher-block-list  option.
              But be aware its implications.
  --client-psk-secrets=<PATH>
              Read PSK identity and secrets from <PATH>.  This is used
              for backend connection.  The each  line of input file is
              formatted  as <identity>:<hex-secret>,  where <identity>
              is PSK identity, and <hex-secret>  is secret in hex.  An
              empty line, and line which  starts with '#' are skipped.
              The first identity and  secret pair encountered is used.
              The default  enabled cipher  list might not  contain any
              PSK  cipher suite.   In  that case,  desired PSK  cipher
              suites  must be  enabled using  --client-ciphers option.
              The  desired PSK  cipher suite  may be  block listed  by
              HTTP/2.   To  use  those   cipher  suites  with  HTTP/2,
              consider   to  use   --client-no-http2-cipher-block-list
              option.  But be aware its implications.
  --tls-no-postpone-early-data
              By  default,   except  for  QUIC   connections,  nghttpx
              postpones forwarding  HTTP requests sent in  early data,
              including  those  sent in  partially  in  it, until  TLS
              handshake  finishes.  If  all backend  server recognizes
              "Early-Data"  header  field,  using  this  option  makes
              nghttpx  not postpone  forwarding request  and get  full
              potential of 0-RTT data.
  --tls-max-early-data=<SIZE>
              Sets  the  maximum  amount  of 0-RTT  data  that  server
              accepts.
              Default: )"
      << util::utos_unit(config->tls.max_early_data) << R"(
  --tls-ktls  Enable   ktls.    For   server,  ktls   is   enable   if
              --tls-session-cache-memcached is not configured.

HTTP/2:
  -c, --frontend-http2-max-concurrent-streams=<N>
              Set the maximum number of  the concurrent streams in one
              frontend HTTP/2 session.
              Default: )"
      << config->http2.upstream.max_concurrent_streams << R"(
  --backend-http2-max-concurrent-streams=<N>
              Set the maximum number of  the concurrent streams in one
              backend  HTTP/2 session.   This sets  maximum number  of
              concurrent opened pushed streams.  The maximum number of
              concurrent requests are set by a remote server.
              Default: )"
      << config->http2.downstream.max_concurrent_streams << R"(
  --frontend-http2-window-size=<SIZE>
              Sets  the  per-stream  initial  window  size  of  HTTP/2
              frontend connection.
              Default: )"
      << config->http2.upstream.window_size << R"(
  --frontend-http2-connection-window-size=<SIZE>
              Sets the  per-connection window size of  HTTP/2 frontend
              connection.
              Default: )"
      << config->http2.upstream.connection_window_size << R"(
  --backend-http2-window-size=<SIZE>
              Sets  the   initial  window   size  of   HTTP/2  backend
              connection.
              Default: )"
      << config->http2.downstream.window_size << R"(
  --backend-http2-connection-window-size=<SIZE>
              Sets the  per-connection window  size of  HTTP/2 backend
              connection.
              Default: )"
      << config->http2.downstream.connection_window_size << R"(
  --http2-no-cookie-crumbling
              Don't crumble cookie header field.
  --padding=<N>
              Add  at most  <N> bytes  to  a HTTP/2  frame payload  as
              padding.  Specify 0 to  disable padding.  This option is
              meant for debugging purpose  and not intended to enhance
              protocol security.
  --no-server-push
              Disable HTTP/2 server push.  Server push is supported by
              default mode and HTTP/2  frontend via Link header field.
              It is  also supported if  both frontend and  backend are
              HTTP/2 in default mode.  In  this case, server push from
              backend session is relayed  to frontend, and server push
              via Link header field is also supported.
  --frontend-http2-optimize-write-buffer-size
              (Experimental) Enable write  buffer size optimization in
              frontend HTTP/2 TLS  connection.  This optimization aims
              to reduce  write buffer  size so  that it  only contains
              bytes  which can  send immediately.   This makes  server
              more responsive to prioritized HTTP/2 stream because the
              buffering  of lower  priority stream  is reduced.   This
              option is only effective on recent Linux platform.
  --frontend-http2-optimize-window-size
              (Experimental)   Automatically  tune   connection  level
              window size of frontend  HTTP/2 TLS connection.  If this
              feature is  enabled, connection window size  starts with
              the   default  window   size,   65535  bytes.    nghttpx
              automatically  adjusts connection  window size  based on
              TCP receiving  window size.  The maximum  window size is
              capped      by      the     value      specified      by
              --frontend-http2-connection-window-size.     Since   the
              stream is subject to stream level window size, it should
              be adjusted using --frontend-http2-window-size option as
              well.   This option  is only  effective on  recent Linux
              platform.
  --frontend-http2-encoder-dynamic-table-size=<SIZE>
              Specify the maximum dynamic  table size of HPACK encoder
              in the frontend HTTP/2 connection.  The decoder (client)
              specifies  the maximum  dynamic table  size it  accepts.
              Then the negotiated dynamic table size is the minimum of
              this option value and the value which client specified.
              Default: )"
      << util::utos_unit(config->http2.upstream.encoder_dynamic_table_size)
      << R"(
  --frontend-http2-decoder-dynamic-table-size=<SIZE>
              Specify the maximum dynamic  table size of HPACK decoder
              in the frontend HTTP/2 connection.
              Default: )"
      << util::utos_unit(config->http2.upstream.decoder_dynamic_table_size)
      << R"(
  --backend-http2-encoder-dynamic-table-size=<SIZE>
              Specify the maximum dynamic  table size of HPACK encoder
              in the backend HTTP/2 connection.  The decoder (backend)
              specifies  the maximum  dynamic table  size it  accepts.
              Then the negotiated dynamic table size is the minimum of
              this option value and the value which backend specified.
              Default: )"
      << util::utos_unit(config->http2.downstream.encoder_dynamic_table_size)
      << R"(
  --backend-http2-decoder-dynamic-table-size=<SIZE>
              Specify the maximum dynamic  table size of HPACK decoder
              in the backend HTTP/2 connection.
              Default: )"
      << util::utos_unit(config->http2.downstream.decoder_dynamic_table_size)
      << R"(

Mode:
  (default mode)
              Accept  HTTP/2,  and  HTTP/1.1 over  SSL/TLS.   "no-tls"
              parameter is  used in  --frontend option,  accept HTTP/2
              and HTTP/1.1 over cleartext  TCP.  The incoming HTTP/1.1
              connection  can  be  upgraded  to  HTTP/2  through  HTTP
              Upgrade.
  -s, --http2-proxy
              Like default mode, but enable forward proxy.  This is so
              called HTTP/2 proxy mode.

Logging:
  -L, --log-level=<LEVEL>
              Set the severity  level of log output.   <LEVEL> must be
              one of INFO, NOTICE, WARN, ERROR and FATAL.
              Default: NOTICE
  --accesslog-file=<PATH>
              Set path to write access log.  To reopen file, send USR1
              signal to nghttpx.
  --accesslog-syslog
              Send  access log  to syslog.   If this  option is  used,
              --accesslog-file option is ignored.
  --accesslog-format=<FORMAT>
              Specify  format  string  for access  log.   The  default
              format is combined format.   The following variables are
              available:

              * $remote_addr: client IP address.
              * $time_local: local time in Common Log format.
              * $time_iso8601: local time in ISO 8601 format.
              * $request: HTTP request line.
              * $status: HTTP response status code.
              * $body_bytes_sent: the  number of bytes sent  to client
                as response body.
              * $http_<VAR>: value of HTTP  request header <VAR> where
                '_' in <VAR> is replaced with '-'.
              * $remote_port: client  port.
              * $server_port: server port.
              * $request_time: request processing time in seconds with
                milliseconds resolution.
              * $pid: PID of the running process.
              * $alpn: ALPN identifier of the protocol which generates
                the response.   For HTTP/1,  ALPN is  always http/1.1,
                regardless of minor version.
              * $tls_cipher: cipher used for SSL/TLS connection.
              * $tls_client_fingerprint_sha256: SHA-256 fingerprint of
                client certificate.
              * $tls_client_fingerprint_sha1:  SHA-1   fingerprint  of
                client certificate.
              * $tls_client_subject_name:   subject  name   in  client
                certificate.
              * $tls_client_issuer_name:   issuer   name   in   client
                certificate.
              * $tls_client_serial:    serial    number   in    client
                certificate.
              * $tls_protocol: protocol for SSL/TLS connection.
              * $tls_session_id: session ID for SSL/TLS connection.
              * $tls_session_reused:  "r"   if  SSL/TLS   session  was
                reused.  Otherwise, "."
              * $tls_sni: SNI server name for SSL/TLS connection.
              * $backend_host:  backend  host   used  to  fulfill  the
                request.  "-" if backend host is not available.
              * $backend_port:  backend  port   used  to  fulfill  the
                request.  "-" if backend host is not available.
              * $method: HTTP method
              * $path:  Request  path  including query.   For  CONNECT
                request, authority is recorded.
              * $path_without_query:  $path   up  to  the   first  '?'
                character.    For   CONNECT  request,   authority   is
                recorded.
              * $protocol_version:   HTTP  version   (e.g.,  HTTP/1.1,
                HTTP/2)

              The  variable  can  be  enclosed  by  "{"  and  "}"  for
              disambiguation (e.g., ${remote_addr}).

              Default: )"
      << DEFAULT_ACCESSLOG_FORMAT << R"(
  --accesslog-write-early
              Write  access  log  when   response  header  fields  are
              received   from  backend   rather   than  when   request
              transaction finishes.
  --errorlog-file=<PATH>
              Set path to write error  log.  To reopen file, send USR1
              signal  to nghttpx.   stderr will  be redirected  to the
              error log file unless --errorlog-syslog is used.
              Default: )"
      << config->logging.error.file << R"(
  --errorlog-syslog
              Send  error log  to  syslog.  If  this  option is  used,
              --errorlog-file option is ignored.
  --syslog-facility=<FACILITY>
              Set syslog facility to <FACILITY>.
              Default: )"
      << str_syslog_facility(config->logging.syslog_facility) << R"(

HTTP:
  --add-x-forwarded-for
              Append  X-Forwarded-For header  field to  the downstream
              request.
  --strip-incoming-x-forwarded-for
              Strip X-Forwarded-For  header field from  inbound client
              requests.
  --no-add-x-forwarded-proto
              Don't append  additional X-Forwarded-Proto  header field
              to  the   backend  request.   If  inbound   client  sets
              X-Forwarded-Proto,                                   and
              --no-strip-incoming-x-forwarded-proto  option  is  used,
              they are passed to the backend.
  --no-strip-incoming-x-forwarded-proto
              Don't strip X-Forwarded-Proto  header field from inbound
              client requests.
  --add-forwarded=<LIST>
              Append RFC  7239 Forwarded header field  with parameters
              specified in comma delimited list <LIST>.  The supported
              parameters  are "by",  "for", "host",  and "proto".   By
              default,  the value  of  "by" and  "for" parameters  are
              obfuscated     string.     See     --forwarded-by    and
              --forwarded-for options respectively.  Note that nghttpx
              does  not  translate non-standard  X-Forwarded-*  header
              fields into Forwarded header field, and vice versa.
  --strip-incoming-forwarded
              Strip  Forwarded   header  field  from   inbound  client
              requests.
  --forwarded-by=(obfuscated|ip|<VALUE>)
              Specify the parameter value sent out with "by" parameter
              of Forwarded  header field.   If "obfuscated"  is given,
              the string is randomly generated at startup.  If "ip" is
              given,   the  interface   address  of   the  connection,
              including port number, is  sent with "by" parameter.  In
              case of UNIX domain  socket, "localhost" is used instead
              of address and  port.  User can also  specify the static
              obfuscated string.  The limitation is that it must start
              with   "_",  and   only   consists   of  character   set
              [A-Za-z0-9._-], as described in RFC 7239.
              Default: obfuscated
  --forwarded-for=(obfuscated|ip)
              Specify  the   parameter  value  sent  out   with  "for"
              parameter of Forwarded header field.  If "obfuscated" is
              given, the string is  randomly generated for each client
              connection.  If "ip" is given, the remote client address
              of  the connection,  without port  number, is  sent with
              "for"  parameter.   In  case   of  UNIX  domain  socket,
              "localhost" is used instead of address.
              Default: obfuscated
  --no-via    Don't append to  Via header field.  If  Via header field
              is received, it is left unaltered.
  --no-strip-incoming-early-data
              Don't strip Early-Data header  field from inbound client
              requests.
  --no-location-rewrite
              Don't  rewrite location  header field  in default  mode.
              When --http2-proxy  is used, location header  field will
              not be altered regardless of this option.
  --host-rewrite
              Rewrite  host and  :authority header  fields in  default
              mode.  When  --http2-proxy is  used, these  headers will
              not be altered regardless of this option.
  --altsvc=<PROTOID,PORT[,HOST,[ORIGIN[,PARAMS]]]>
              Specify   protocol  ID,   port,  host   and  origin   of
              alternative service.  <HOST>,  <ORIGIN> and <PARAMS> are
              optional.   Empty <HOST>  and <ORIGIN>  are allowed  and
              they  are treated  as  nothing is  specified.  They  are
              advertised  in alt-svc  header  field  only in  HTTP/1.1
              frontend.   This option  can be  used multiple  times to
              specify multiple alternative services.
              Example: --altsvc="h2,443,,,ma=3600; persist=1"
  --http2-altsvc=<PROTOID,PORT[,HOST,[ORIGIN[,PARAMS]]]>
              Just like --altsvc option, but  this altsvc is only sent
              in HTTP/2 frontend.
  --add-request-header=<HEADER>
              Specify additional header field to add to request header
              set.   The field  name must  be lowercase.   This option
              just  appends header  field and  won't replace  anything
              already set.  This  option can be used  several times to
              specify multiple header fields.
              Example: --add-request-header="foo: bar"
  --add-response-header=<HEADER>
              Specify  additional  header  field to  add  to  response
              header  set.  The  field name  must be  lowercase.  This
              option  just  appends  header field  and  won't  replace
              anything already  set.  This option can  be used several
              times to specify multiple header fields.
              Example: --add-response-header="foo: bar"
  --request-header-field-buffer=<SIZE>
              Set maximum buffer size for incoming HTTP request header
              field list.  This is the sum of header name and value in
              bytes.   If  trailer  fields  exist,  they  are  counted
              towards this number.
              Default: )"
      << util::utos_unit(config->http.request_header_field_buffer) << R"(
  --max-request-header-fields=<N>
              Set  maximum  number  of incoming  HTTP  request  header
              fields.   If  trailer  fields exist,  they  are  counted
              towards this number.
              Default: )"
      << config->http.max_request_header_fields << R"(
  --response-header-field-buffer=<SIZE>
              Set  maximum  buffer  size for  incoming  HTTP  response
              header field list.   This is the sum of  header name and
              value  in  bytes.  If  trailer  fields  exist, they  are
              counted towards this number.
              Default: )"
      << util::utos_unit(config->http.response_header_field_buffer) << R"(
  --max-response-header-fields=<N>
              Set  maximum number  of  incoming  HTTP response  header
              fields.   If  trailer  fields exist,  they  are  counted
              towards this number.
              Default: )"
      << config->http.max_response_header_fields << R"(
  --error-page=(<CODE>|*)=<PATH>
              Set file path  to custom error page  served when nghttpx
              originally  generates  HTTP  error status  code  <CODE>.
              <CODE> must be greater than or equal to 400, and at most
              599.  If "*"  is used instead of <CODE>,  it matches all
              HTTP  status  code.  If  error  status  code comes  from
              backend server, the custom error pages are not used.
  --server-name=<NAME>
              Change server response header field value to <NAME>.
              Default: )"
      << config->http.server_name << R"(
  --no-server-rewrite
              Don't rewrite server header field in default mode.  When
              --http2-proxy is used, these headers will not be altered
              regardless of this option.
  --redirect-https-port=<PORT>
              Specify the port number which appears in Location header
              field  when  redirect  to  HTTPS  URI  is  made  due  to
              "redirect-if-not-tls" parameter in --backend option.
              Default: )"
      << config->http.redirect_https_port << R"(
  --require-http-scheme
              Always require http or https scheme in HTTP request.  It
              also  requires that  https scheme  must be  used for  an
              encrypted  connection.  Otherwise,  http scheme  must be
              used.   This   option  is   recommended  for   a  server
              deployment which directly faces clients and the services
              it provides only require http or https scheme.

API:
  --api-max-request-body=<SIZE>
              Set the maximum size of request body for API request.
              Default: )"
      << util::utos_unit(config->api.max_request_body) << R"(

DNS:
  --dns-cache-timeout=<DURATION>
              Set duration that cached DNS results remain valid.  Note
              that nghttpx caches the unsuccessful results as well.
              Default: )"
      << util::duration_str(config->dns.timeout.cache) << R"(
  --dns-lookup-timeout=<DURATION>
              Set timeout that  DNS server is given to  respond to the
              initial  DNS  query.  For  the  2nd  and later  queries,
              server is  given time based  on this timeout, and  it is
              scaled linearly.
              Default: )"
      << util::duration_str(config->dns.timeout.lookup) << R"(
  --dns-max-try=<N>
              Set the number of DNS query before nghttpx gives up name
              lookup.
              Default: )"
      << config->dns.max_try << R"(
  --frontend-max-requests=<N>
              The number  of requests that single  frontend connection
              can process.  For HTTP/2, this  is the number of streams
              in  one  HTTP/2 connection.   For  HTTP/1,  this is  the
              number of keep alive requests.  This is hint to nghttpx,
              and it  may allow additional few  requests.  The default
              value is unlimited.

Debug:
  --frontend-http2-dump-request-header=<PATH>
              Dumps request headers received by HTTP/2 frontend to the
              file denoted  in <PATH>.  The  output is done  in HTTP/1
              header field format and each header block is followed by
              an empty line.  This option  is not thread safe and MUST
              NOT be used with option -n<N>, where <N> >= 2.
  --frontend-http2-dump-response-header=<PATH>
              Dumps response headers sent  from HTTP/2 frontend to the
              file denoted  in <PATH>.  The  output is done  in HTTP/1
              header field format and each header block is followed by
              an empty line.  This option  is not thread safe and MUST
              NOT be used with option -n<N>, where <N> >= 2.
  -o, --frontend-frame-debug
              Print HTTP/2 frames in  frontend to stderr.  This option
              is  not thread  safe and  MUST NOT  be used  with option
              -n=N, where N >= 2.

Process:
  -D, --daemon
              Run in a background.  If -D is used, the current working
              directory is changed to '/'.
  --pid-file=<PATH>
              Set path to save PID of this program.
  --user=<USER>
              Run this program as <USER>.   This option is intended to
              be used to drop root privileges.
  --single-process
              Run this program in a  single process mode for debugging
              purpose.  Without this option,  nghttpx creates at least
              2 processes: main and  worker processes.  If this option
              is  used, main  and  worker are  unified  into a  single
              process.   nghttpx still  spawns  additional process  if
              neverbleed  is used.   In the  single process  mode, the
              signal handling feature is disabled.
  --max-worker-processes=<N>
              The maximum number of  worker processes.  nghttpx spawns
              new worker  process when  it reloads  its configuration.
              The previous worker  process enters graceful termination
              period and will terminate  when it finishes handling the
              existing    connections.     However,    if    reloading
              configurations  happen   very  frequently,   the  worker
              processes might be piled up if they take a bit long time
              to finish  the existing connections.  With  this option,
              if  the number  of  worker processes  exceeds the  given
              value,   the  oldest   worker   process  is   terminated
              immediately.  Specifying 0 means no  limit and it is the
              default behaviour.
  --worker-process-grace-shutdown-period=<DURATION>
              Maximum  period  for  a   worker  process  to  terminate
              gracefully.  When  a worker  process enters  in graceful
              shutdown   period  (e.g.,   when  nghttpx   reloads  its
              configuration)  and  it  does not  finish  handling  the
              existing connections in the given  period of time, it is
              immediately terminated.  Specifying 0 means no limit and
              it is the default behaviour.

Scripting:
  --mruby-file=<PATH>
              Set mruby script file
  --ignore-per-pattern-mruby-error
              Ignore mruby compile error  for per-pattern mruby script
              file.  If error  occurred, it is treated as  if no mruby
              file were specified for the pattern.
)";

#ifdef ENABLE_HTTP3
  out << R"(
HTTP/3 and QUIC:
  --frontend-quic-idle-timeout=<DURATION>
              Specify an idle timeout for QUIC connection.
              Default: )"
      << util::duration_str(config->quic.upstream.timeout.idle) << R"(
  --frontend-quic-debug-log
              Output QUIC debug log to /dev/stderr.
  --quic-bpf-program-file=<PATH>
              Specify a path to  eBPF program file reuseport_kern.o to
              direct  an  incoming  QUIC  UDP datagram  to  a  correct
              socket.
              Default: )"
      << config->quic.bpf.prog_file << R"(
  --frontend-quic-early-data
              Enable early data on frontend QUIC connections.  nghttpx
              sends "Early-Data" header field to a backend server if a
              request is received in early  data and handshake has not
              finished.  All backend servers should deal with possibly
              replayed requests.
  --frontend-quic-qlog-dir=<DIR>
              Specify a  directory where  a qlog  file is  written for
              frontend QUIC  connections.  A qlog file  is created per
              each QUIC  connection.  The  file name is  ISO8601 basic
              format, followed by "-", server Source Connection ID and
              ".sqlog".
  --frontend-quic-require-token
              Require an address validation  token for a frontend QUIC
              connection.   Server sends  a token  in Retry  packet or
              NEW_TOKEN frame in the previous connection.
  --frontend-quic-congestion-controller=<CC>
              Specify a congestion controller algorithm for a frontend
              QUIC  connection.   <CC>  should be  either  "cubic"  or
              "bbr".
              Default: )"
      << (config->quic.upstream.congestion_controller == NGTCP2_CC_ALGO_CUBIC
            ? "cubic"
            : "bbr")
      << R"(
  --frontend-quic-secret-file=<PATH>
              Path to file that contains secure random data to be used
              as QUIC keying materials.  It is used to derive keys for
              encrypting tokens and Connection IDs.  It is not used to
              encrypt  QUIC  packets.  Each  line  of  this file  must
              contain  exactly  136  bytes  hex-encoded  string  (when
              decoded the byte string is  68 bytes long).  The first 3
              bits of  decoded byte  string are  used to  identify the
              keying material.  An  empty line or a  line which starts
              '#'  is ignored.   The file  can contain  more than  one
              keying materials.  Because the  identifier is 3 bits, at
              most 8 keying materials are  read and the remaining data
              is discarded.  The first keying  material in the file is
              primarily  used for  encryption and  decryption for  new
              connection.  The other ones are used to decrypt data for
              the  existing connections.   Specifying multiple  keying
              materials enables  key rotation.   Please note  that key
              rotation  does  not  occur automatically.   User  should
              update  files  or  change  options  values  and  restart
              nghttpx gracefully.   If opening  or reading  given file
              fails, all loaded keying  materials are discarded and it
              is treated as if none of  this option is given.  If this
              option is not  given or an error  occurred while opening
              or  reading  a  file,  a keying  material  is  generated
              internally on startup and reload.
  --quic-server-id=<HEXSTRING>
              Specify server  ID encoded in Connection  ID to identify
              this  particular  server  instance.   Connection  ID  is
              encrypted and  this part is  not visible in  public.  It
              must be 4  bytes long and must be encoded  in hex string
              (which is 8  bytes long).  If this option  is omitted, a
              random   server  ID   is   generated   on  startup   and
              configuration reload.
  --frontend-quic-initial-rtt=<DURATION>
              Specify the initial RTT of the frontend QUIC connection.
              Default: )"
      << util::duration_str(config->quic.upstream.initial_rtt) << R"(
  --no-quic-bpf
              Disable eBPF.
  --frontend-http3-window-size=<SIZE>
              Sets  the  per-stream  initial  window  size  of  HTTP/3
              frontend connection.
              Default: )"
      << util::utos_unit(config->http3.upstream.window_size) << R"(
  --frontend-http3-connection-window-size=<SIZE>
              Sets the  per-connection window size of  HTTP/3 frontend
              connection.
              Default: )"
      << util::utos_unit(config->http3.upstream.connection_window_size) << R"(
  --frontend-http3-max-window-size=<SIZE>
              Sets  the  maximum  per-stream  window  size  of  HTTP/3
              frontend connection.  The window  size is adjusted based
              on the receiving rate of stream data.  The initial value
              is the  value specified  by --frontend-http3-window-size
              and the window size grows up to <SIZE> bytes.
              Default: )"
      << util::utos_unit(config->http3.upstream.max_window_size) << R"(
  --frontend-http3-max-connection-window-size=<SIZE>
              Sets the  maximum per-connection  window size  of HTTP/3
              frontend connection.  The window  size is adjusted based
              on the receiving rate of stream data.  The initial value
              is         the         value        specified         by
              --frontend-http3-connection-window-size  and the  window
              size grows up to <SIZE> bytes.
              Default: )"
      << util::utos_unit(config->http3.upstream.max_connection_window_size)
      << R"(
  --frontend-http3-max-concurrent-streams=<N>
              Set the maximum number of  the concurrent streams in one
              frontend HTTP/3 connection.
              Default: )"
      << config->http3.upstream.max_concurrent_streams << R"(
)";
#endif // ENABLE_HTTP3

  out << R"(
Misc:
  --conf=<PATH>
              Load  configuration  from   <PATH>.   Please  note  that
              nghttpx always  tries to read the  default configuration
              file if --conf is not given.
              Default: )"
      << config->conf_path << R"(
  --include=<PATH>
              Load additional configurations from <PATH>.  File <PATH>
              is  read  when  configuration  parser  encountered  this
              option.  This option can be used multiple times, or even
              recursively.
  -v, --version
              Print version and exit.
  -h, --help  Print this help and exit.

--

  The <SIZE> argument is an integer and an optional unit (e.g., 10K is
  10 * 1024).  Units are K, M and G (powers of 1024).

  The <DURATION> argument is an integer and an optional unit (e.g., 1s
  is 1 second and 500ms is 500 milliseconds).  Units are h, m, s or ms
  (hours, minutes, seconds and milliseconds, respectively).  If a unit
  is omitted, a second is used as unit.)"
      << std::endl;
}
} // namespace

namespace {
int process_options(Config *config,
                    std::vector<std::pair<StringRef, StringRef>> &cmdcfgs) {
  std::array<char, STRERROR_BUFSIZE> errbuf;
  std::map<StringRef, size_t> pattern_addr_indexer;
  if (conf_exists(config->conf_path.data())) {
    LOG(NOTICE) << "Loading configuration from " << config->conf_path;
    std::set<StringRef> include_set;
    if (load_config(config, config->conf_path.data(), include_set,
                    pattern_addr_indexer) == -1) {
      LOG(FATAL) << "Failed to load configuration from " << config->conf_path;
      return -1;
    }
    assert(include_set.empty());
  }

  // Reopen log files using configurations in file
  reopen_log_files(config->logging);

  {
    std::set<StringRef> include_set;

    for (auto &p : cmdcfgs) {
      if (parse_config(config, p.first, p.second, include_set,
                       pattern_addr_indexer) == -1) {
        LOG(FATAL) << "Failed to parse command-line argument.";
        return -1;
      }
    }

    assert(include_set.empty());
  }

  Log::set_severity_level(config->logging.severity);

  auto &loggingconf = config->logging;

  if (loggingconf.access.syslog || loggingconf.error.syslog) {
    openlog("nghttpx", LOG_NDELAY | LOG_NOWAIT | LOG_PID,
            loggingconf.syslog_facility);
  }

  if (reopen_log_files(config->logging) != 0) {
    LOG(FATAL) << "Failed to open log file";
    return -1;
  }

  redirect_stderr_to_errorlog(loggingconf);

  if (config->uid != 0) {
    if (log_config()->accesslog_fd != -1 &&
        fchown(log_config()->accesslog_fd, config->uid, config->gid) == -1) {
      auto error = errno;
      LOG(WARN) << "Changing owner of access log file failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
    if (log_config()->errorlog_fd != -1 &&
        fchown(log_config()->errorlog_fd, config->uid, config->gid) == -1) {
      auto error = errno;
      LOG(WARN) << "Changing owner of error log file failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
  }

  if (config->single_thread) {
    LOG(WARN) << "single-thread: Set workers to 1";
    config->num_worker = 1;
  }

  auto &http2conf = config->http2;
  {
    auto &dumpconf = http2conf.upstream.debug.dump;

    if (!dumpconf.request_header_file.empty()) {
      auto path = dumpconf.request_header_file.data();
      auto f = open_file_for_write(path);

      if (f == nullptr) {
        LOG(FATAL) << "Failed to open http2 upstream request header file: "
                   << path;
        return -1;
      }

      dumpconf.request_header = f;

      if (config->uid != 0) {
        if (chown(path, config->uid, config->gid) == -1) {
          auto error = errno;
          LOG(WARN) << "Changing owner of http2 upstream request header file "
                    << path << " failed: "
                    << xsi_strerror(error, errbuf.data(), errbuf.size());
        }
      }
    }

    if (!dumpconf.response_header_file.empty()) {
      auto path = dumpconf.response_header_file.data();
      auto f = open_file_for_write(path);

      if (f == nullptr) {
        LOG(FATAL) << "Failed to open http2 upstream response header file: "
                   << path;
        return -1;
      }

      dumpconf.response_header = f;

      if (config->uid != 0) {
        if (chown(path, config->uid, config->gid) == -1) {
          auto error = errno;
          LOG(WARN) << "Changing owner of http2 upstream response header file"
                    << " " << path << " failed: "
                    << xsi_strerror(error, errbuf.data(), errbuf.size());
        }
      }
    }
  }

  auto &tlsconf = config->tls;

  if (tlsconf.alpn_list.empty()) {
    tlsconf.alpn_list = util::split_str(DEFAULT_ALPN_LIST, ',');
  }

  if (!tlsconf.tls_proto_list.empty()) {
    tlsconf.tls_proto_mask = tls::create_tls_proto_mask(tlsconf.tls_proto_list);
  }

  // TODO We depends on the ordering of protocol version macro in
  // OpenSSL.
  if (tlsconf.min_proto_version > tlsconf.max_proto_version) {
    LOG(ERROR) << "tls-max-proto-version must be equal to or larger than "
                  "tls-min-proto-version";
    return -1;
  }

  if (tls::set_alpn_prefs(tlsconf.alpn_prefs, tlsconf.alpn_list) != 0) {
    return -1;
  }

#if defined(NGHTTP2_GENUINE_OPENSSL) ||                                        \
  defined(NGHTTP2_OPENSSL_IS_BORINGSSL) ||                                     \
  defined(NGHTTP2_OPENSSL_IS_LIBRESSL)
  tlsconf.bio_method = create_bio_method();
#endif // NGHTTP2_GENUINE_OPENSSL || NGHTTP2_OPENSSL_IS_BORINGSSL ||
       // NGHTTP2_OPENSSL_IS_LIBRESSL

  auto &listenerconf = config->conn.listener;
  auto &upstreamconf = config->conn.upstream;

  if (listenerconf.addrs.empty()) {
    UpstreamAddr addr{};
    addr.host = "*"_sr;
    addr.port = 3000;
    addr.tls = true;
    addr.family = AF_INET;
    addr.index = 0;
    listenerconf.addrs.push_back(addr);
    addr.family = AF_INET6;
    addr.index = 1;
    listenerconf.addrs.push_back(std::move(addr));
  }

  if (upstreamconf.worker_connections == 0) {
    upstreamconf.worker_connections = std::numeric_limits<size_t>::max();
  }

  if (tls::upstream_tls_enabled(config->conn) &&
      (tlsconf.private_key_file.empty() || tlsconf.cert_file.empty())) {
    LOG(FATAL) << "TLS private key and certificate files are required.  "
                  "Specify them in command-line, or in configuration file "
                  "using private-key-file and certificate-file options.";
    return -1;
  }

  if (tls::upstream_tls_enabled(config->conn) && !tlsconf.ocsp.disabled) {
    struct stat buf;
    if (stat(tlsconf.ocsp.fetch_ocsp_response_file.data(), &buf) != 0) {
      tlsconf.ocsp.disabled = true;
      LOG(WARN) << "--fetch-ocsp-response-file: "
                << tlsconf.ocsp.fetch_ocsp_response_file
                << " not found.  OCSP stapling has been disabled.";
    }
  }

  if (configure_downstream_group(config, config->http2_proxy, false, tlsconf) !=
      0) {
    return -1;
  }

  std::array<char, util::max_hostport> hostport_buf;

  auto &proxy = config->downstream_http_proxy;
  if (!proxy.host.empty()) {
    auto hostport = util::make_hostport(std::begin(hostport_buf),
                                        StringRef{proxy.host}, proxy.port);
    if (resolve_hostname(&proxy.addr, proxy.host.data(), proxy.port,
                         AF_UNSPEC) == -1) {
      LOG(FATAL) << "Resolving backend HTTP proxy address failed: " << hostport;
      return -1;
    }
    LOG(NOTICE) << "Backend HTTP proxy address: " << hostport << " -> "
                << util::to_numeric_addr(&proxy.addr);
  }

  {
    auto &memcachedconf = tlsconf.session_cache.memcached;
    if (!memcachedconf.host.empty()) {
      auto hostport =
        util::make_hostport(std::begin(hostport_buf),
                            StringRef{memcachedconf.host}, memcachedconf.port);
      if (resolve_hostname(&memcachedconf.addr, memcachedconf.host.data(),
                           memcachedconf.port, memcachedconf.family) == -1) {
        LOG(FATAL)
          << "Resolving memcached address for TLS session cache failed: "
          << hostport;
        return -1;
      }
      LOG(NOTICE) << "Memcached address for TLS session cache: " << hostport
                  << " -> " << util::to_numeric_addr(&memcachedconf.addr);
      if (memcachedconf.tls) {
        LOG(NOTICE) << "Connection to memcached for TLS session cache will be "
                       "encrypted by TLS";
      }
    }
  }

  {
    auto &memcachedconf = tlsconf.ticket.memcached;
    if (!memcachedconf.host.empty()) {
      auto hostport =
        util::make_hostport(std::begin(hostport_buf),
                            StringRef{memcachedconf.host}, memcachedconf.port);
      if (resolve_hostname(&memcachedconf.addr, memcachedconf.host.data(),
                           memcachedconf.port, memcachedconf.family) == -1) {
        LOG(FATAL) << "Resolving memcached address for TLS ticket key failed: "
                   << hostport;
        return -1;
      }
      LOG(NOTICE) << "Memcached address for TLS ticket key: " << hostport
                  << " -> " << util::to_numeric_addr(&memcachedconf.addr);
      if (memcachedconf.tls) {
        LOG(NOTICE) << "Connection to memcached for TLS ticket key will be "
                       "encrypted by TLS";
      }
    }
  }

  if (config->rlimit_nofile) {
    struct rlimit lim = {static_cast<rlim_t>(config->rlimit_nofile),
                         static_cast<rlim_t>(config->rlimit_nofile)};
    if (setrlimit(RLIMIT_NOFILE, &lim) != 0) {
      auto error = errno;
      LOG(WARN) << "Setting rlimit-nofile failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
  }

#ifdef RLIMIT_MEMLOCK
  if (config->rlimit_memlock) {
    struct rlimit lim = {static_cast<rlim_t>(config->rlimit_memlock),
                         static_cast<rlim_t>(config->rlimit_memlock)};
    if (setrlimit(RLIMIT_MEMLOCK, &lim) != 0) {
      auto error = errno;
      LOG(WARN) << "Setting rlimit-memlock failed: "
                << xsi_strerror(error, errbuf.data(), errbuf.size());
    }
  }
#endif // RLIMIT_MEMLOCK

  auto &fwdconf = config->http.forwarded;

  if (fwdconf.by_node_type == ForwardedNode::OBFUSCATED &&
      fwdconf.by_obfuscated.empty()) {
    // 2 for '_' and terminal NULL
    auto iov = make_byte_ref(config->balloc, SHRPX_OBFUSCATED_NODE_LENGTH + 2);
    auto p = std::begin(iov);
    *p++ = '_';
    auto gen = util::make_mt19937();
    p = util::random_alpha_digit(p, p + SHRPX_OBFUSCATED_NODE_LENGTH, gen);
    *p = '\0';
    fwdconf.by_obfuscated = StringRef{std::span{std::begin(iov), p}};
  }

  if (config->http2.upstream.debug.frame_debug) {
    // To make it sync to logging
    set_output(stderr);
    if (isatty(fileno(stdout))) {
      set_color_output(true);
    }
    reset_timer();
  }

  config->http2.upstream.callbacks = create_http2_upstream_callbacks();
  config->http2.downstream.callbacks = create_http2_downstream_callbacks();

  if (!config->http.altsvcs.empty()) {
    config->http.altsvc_header_value =
      http::create_altsvc_header_value(config->balloc, config->http.altsvcs);
  }

  if (!config->http.http2_altsvcs.empty()) {
    config->http.http2_altsvc_header_value = http::create_altsvc_header_value(
      config->balloc, config->http.http2_altsvcs);
  }

  return 0;
}
} // namespace

namespace {
// Closes file descriptor which are opened for listeners in config,
// and are not inherited from |iaddrs|.
void close_not_inherited_fd(Config *config,
                            const std::vector<InheritedAddr> &iaddrs) {
  auto &listenerconf = config->conn.listener;

  for (auto &addr : listenerconf.addrs) {
    auto inherited = std::find_if(
      std::begin(iaddrs), std::end(iaddrs),
      [&addr](const InheritedAddr &iaddr) { return addr.fd == iaddr.fd; });

    if (inherited != std::end(iaddrs)) {
      continue;
    }

    close(addr.fd);
  }
}
} // namespace

namespace {
void reload_config() {
  int rv;

  LOG(NOTICE) << "Reloading configuration";

  auto cur_config = mod_config();
  auto new_config = std::make_unique<Config>();

  fill_default_config(new_config.get());

  new_config->conf_path =
    make_string_ref(new_config->balloc, cur_config->conf_path);
  // daemon option is ignored here.
  new_config->daemon = cur_config->daemon;
  // loop is reused, and ev_loop_flags gets ignored
  new_config->ev_loop_flags = cur_config->ev_loop_flags;
  new_config->config_revision = cur_config->config_revision + 1;

  rv = process_options(new_config.get(), suconfig.cmdcfgs);
  if (rv != 0) {
    LOG(ERROR) << "Failed to process new configuration";
    return;
  }

  auto iaddrs = get_inherited_addr_from_config(new_config->balloc, cur_config);

  if (create_acceptor_socket(new_config.get(), iaddrs) != 0) {
    close_not_inherited_fd(new_config.get(), iaddrs);
    return;
  }

  // According to libev documentation, flags are ignored since we have
  // already created first default loop.
  auto loop = ev_default_loop(new_config->ev_loop_flags);

  int ipc_fd = 0;
#ifdef ENABLE_HTTP3
  int quic_ipc_fd = 0;

  auto quic_lwps = collect_quic_lingering_worker_processes();

  std::vector<WorkerID> worker_ids;

  if (generate_worker_id(worker_ids, worker_process_seq, new_config.get()) !=
      0) {
    close_not_inherited_fd(new_config.get(), iaddrs);
    return;
  }
#endif // ENABLE_HTTP3

  // fork_worker_process and forked child process assumes new
  // configuration can be obtained from get_config().

  auto old_config = replace_config(std::move(new_config));

  auto pid = fork_worker_process(ipc_fd
#ifdef ENABLE_HTTP3
                                 ,
                                 quic_ipc_fd
#endif // ENABLE_HTTP3

                                 ,
                                 iaddrs
#ifdef ENABLE_HTTP3
                                 ,
                                 worker_ids, std::move(quic_lwps)
#endif // ENABLE_HTTP3
  );

  if (pid == -1) {
    LOG(ERROR) << "Failed to process new configuration";

    new_config = replace_config(std::move(old_config));
    close_not_inherited_fd(new_config.get(), iaddrs);

    return;
  }

  close_unused_inherited_addr(iaddrs);

  worker_process_add(std::make_unique<WorkerProcess>(
    loop, pid, ipc_fd
#ifdef ENABLE_HTTP3
    ,
    quic_ipc_fd, std::move(worker_ids), worker_process_seq++
#endif // ENABLE_HTTP3
    ));

  worker_process_adjust_limit();

  if (!get_config()->pid_file.empty()) {
    save_pid();
  }
}
} // namespace

int main(int argc, char **argv) {
  int rv;
  std::array<char, STRERROR_BUFSIZE> errbuf;

#ifdef HAVE_LIBBPF
  libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
#endif // HAVE_LIBBPF

  Log::set_severity_level(NOTICE);
  create_config();
  fill_default_config(mod_config());

  // make copy of stderr
  store_original_fds();

  // First open log files with default configuration, so that we can
  // log errors/warnings while reading configuration files.
  reopen_log_files(get_config()->logging);

  suconfig.original_argv = argv;

  // We have to copy argv, since getopt_long may change its content.
  suconfig.argc = argc;
  suconfig.argv = new char *[argc];

  for (int i = 0; i < argc; ++i) {
    suconfig.argv[i] = strdup(argv[i]);
    if (suconfig.argv[i] == nullptr) {
      auto error = errno;
      LOG(FATAL) << "failed to copy argv: "
                 << xsi_strerror(error, errbuf.data(), errbuf.size());
      exit(EXIT_FAILURE);
    }
  }

  suconfig.cwd = getcwd(nullptr, 0);
  if (suconfig.cwd == nullptr) {
    auto error = errno;
    LOG(FATAL) << "failed to get current working directory: errno=" << error;
    exit(EXIT_FAILURE);
  }

  auto &cmdcfgs = suconfig.cmdcfgs;

  while (1) {
    static int flag = 0;
    static constexpr option long_options[] = {
      {SHRPX_OPT_DAEMON.data(), no_argument, nullptr, 'D'},
      {SHRPX_OPT_LOG_LEVEL.data(), required_argument, nullptr, 'L'},
      {SHRPX_OPT_BACKEND.data(), required_argument, nullptr, 'b'},
      {SHRPX_OPT_HTTP2_MAX_CONCURRENT_STREAMS.data(), required_argument,
       nullptr, 'c'},
      {SHRPX_OPT_FRONTEND.data(), required_argument, nullptr, 'f'},
      {"help", no_argument, nullptr, 'h'},
      {SHRPX_OPT_INSECURE.data(), no_argument, nullptr, 'k'},
      {SHRPX_OPT_WORKERS.data(), required_argument, nullptr, 'n'},
      {SHRPX_OPT_CLIENT_PROXY.data(), no_argument, nullptr, 'p'},
      {SHRPX_OPT_HTTP2_PROXY.data(), no_argument, nullptr, 's'},
      {"version", no_argument, nullptr, 'v'},
      {SHRPX_OPT_FRONTEND_FRAME_DEBUG.data(), no_argument, nullptr, 'o'},
      {SHRPX_OPT_ADD_X_FORWARDED_FOR.data(), no_argument, &flag, 1},
      {SHRPX_OPT_FRONTEND_HTTP2_READ_TIMEOUT.data(), required_argument, &flag,
       2},
      {SHRPX_OPT_FRONTEND_READ_TIMEOUT.data(), required_argument, &flag, 3},
      {SHRPX_OPT_FRONTEND_WRITE_TIMEOUT.data(), required_argument, &flag, 4},
      {SHRPX_OPT_BACKEND_READ_TIMEOUT.data(), required_argument, &flag, 5},
      {SHRPX_OPT_BACKEND_WRITE_TIMEOUT.data(), required_argument, &flag, 6},
      {SHRPX_OPT_ACCESSLOG_FILE.data(), required_argument, &flag, 7},
      {SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT.data(), required_argument, &flag,
       8},
      {SHRPX_OPT_FRONTEND_HTTP2_WINDOW_BITS.data(), required_argument, &flag,
       9},
      {SHRPX_OPT_PID_FILE.data(), required_argument, &flag, 10},
      {SHRPX_OPT_USER.data(), required_argument, &flag, 11},
      {"conf", required_argument, &flag, 12},
      {SHRPX_OPT_SYSLOG_FACILITY.data(), required_argument, &flag, 14},
      {SHRPX_OPT_BACKLOG.data(), required_argument, &flag, 15},
      {SHRPX_OPT_CIPHERS.data(), required_argument, &flag, 16},
      {SHRPX_OPT_CLIENT.data(), no_argument, &flag, 17},
      {SHRPX_OPT_BACKEND_HTTP2_WINDOW_BITS.data(), required_argument, &flag,
       18},
      {SHRPX_OPT_CACERT.data(), required_argument, &flag, 19},
      {SHRPX_OPT_BACKEND_IPV4.data(), no_argument, &flag, 20},
      {SHRPX_OPT_BACKEND_IPV6.data(), no_argument, &flag, 21},
      {SHRPX_OPT_PRIVATE_KEY_PASSWD_FILE.data(), required_argument, &flag, 22},
      {SHRPX_OPT_NO_VIA.data(), no_argument, &flag, 23},
      {SHRPX_OPT_SUBCERT.data(), required_argument, &flag, 24},
      {SHRPX_OPT_HTTP2_BRIDGE.data(), no_argument, &flag, 25},
      {SHRPX_OPT_BACKEND_HTTP_PROXY_URI.data(), required_argument, &flag, 26},
      {SHRPX_OPT_BACKEND_NO_TLS.data(), no_argument, &flag, 27},
      {SHRPX_OPT_OCSP_STARTUP.data(), no_argument, &flag, 28},
      {SHRPX_OPT_FRONTEND_NO_TLS.data(), no_argument, &flag, 29},
      {SHRPX_OPT_NO_VERIFY_OCSP.data(), no_argument, &flag, 30},
      {SHRPX_OPT_BACKEND_TLS_SNI_FIELD.data(), required_argument, &flag, 31},
      {SHRPX_OPT_DH_PARAM_FILE.data(), required_argument, &flag, 33},
      {SHRPX_OPT_READ_RATE.data(), required_argument, &flag, 34},
      {SHRPX_OPT_READ_BURST.data(), required_argument, &flag, 35},
      {SHRPX_OPT_WRITE_RATE.data(), required_argument, &flag, 36},
      {SHRPX_OPT_WRITE_BURST.data(), required_argument, &flag, 37},
      {SHRPX_OPT_NPN_LIST.data(), required_argument, &flag, 38},
      {SHRPX_OPT_VERIFY_CLIENT.data(), no_argument, &flag, 39},
      {SHRPX_OPT_VERIFY_CLIENT_CACERT.data(), required_argument, &flag, 40},
      {SHRPX_OPT_CLIENT_PRIVATE_KEY_FILE.data(), required_argument, &flag, 41},
      {SHRPX_OPT_CLIENT_CERT_FILE.data(), required_argument, &flag, 42},
      {SHRPX_OPT_FRONTEND_HTTP2_DUMP_REQUEST_HEADER.data(), required_argument,
       &flag, 43},
      {SHRPX_OPT_FRONTEND_HTTP2_DUMP_RESPONSE_HEADER.data(), required_argument,
       &flag, 44},
      {SHRPX_OPT_HTTP2_NO_COOKIE_CRUMBLING.data(), no_argument, &flag, 45},
      {SHRPX_OPT_FRONTEND_HTTP2_CONNECTION_WINDOW_BITS.data(),
       required_argument, &flag, 46},
      {SHRPX_OPT_BACKEND_HTTP2_CONNECTION_WINDOW_BITS.data(), required_argument,
       &flag, 47},
      {SHRPX_OPT_TLS_PROTO_LIST.data(), required_argument, &flag, 48},
      {SHRPX_OPT_PADDING.data(), required_argument, &flag, 49},
      {SHRPX_OPT_WORKER_READ_RATE.data(), required_argument, &flag, 50},
      {SHRPX_OPT_WORKER_READ_BURST.data(), required_argument, &flag, 51},
      {SHRPX_OPT_WORKER_WRITE_RATE.data(), required_argument, &flag, 52},
      {SHRPX_OPT_WORKER_WRITE_BURST.data(), required_argument, &flag, 53},
      {SHRPX_OPT_ALTSVC.data(), required_argument, &flag, 54},
      {SHRPX_OPT_ADD_RESPONSE_HEADER.data(), required_argument, &flag, 55},
      {SHRPX_OPT_WORKER_FRONTEND_CONNECTIONS.data(), required_argument, &flag,
       56},
      {SHRPX_OPT_ACCESSLOG_SYSLOG.data(), no_argument, &flag, 57},
      {SHRPX_OPT_ERRORLOG_FILE.data(), required_argument, &flag, 58},
      {SHRPX_OPT_ERRORLOG_SYSLOG.data(), no_argument, &flag, 59},
      {SHRPX_OPT_STREAM_READ_TIMEOUT.data(), required_argument, &flag, 60},
      {SHRPX_OPT_STREAM_WRITE_TIMEOUT.data(), required_argument, &flag, 61},
      {SHRPX_OPT_NO_LOCATION_REWRITE.data(), no_argument, &flag, 62},
      {SHRPX_OPT_BACKEND_HTTP1_CONNECTIONS_PER_HOST.data(), required_argument,
       &flag, 63},
      {SHRPX_OPT_LISTENER_DISABLE_TIMEOUT.data(), required_argument, &flag, 64},
      {SHRPX_OPT_STRIP_INCOMING_X_FORWARDED_FOR.data(), no_argument, &flag, 65},
      {SHRPX_OPT_ACCESSLOG_FORMAT.data(), required_argument, &flag, 66},
      {SHRPX_OPT_BACKEND_HTTP1_CONNECTIONS_PER_FRONTEND.data(),
       required_argument, &flag, 67},
      {SHRPX_OPT_TLS_TICKET_KEY_FILE.data(), required_argument, &flag, 68},
      {SHRPX_OPT_RLIMIT_NOFILE.data(), required_argument, &flag, 69},
      {SHRPX_OPT_BACKEND_RESPONSE_BUFFER.data(), required_argument, &flag, 71},
      {SHRPX_OPT_BACKEND_REQUEST_BUFFER.data(), required_argument, &flag, 72},
      {SHRPX_OPT_NO_HOST_REWRITE.data(), no_argument, &flag, 73},
      {SHRPX_OPT_NO_SERVER_PUSH.data(), no_argument, &flag, 74},
      {SHRPX_OPT_BACKEND_HTTP2_CONNECTIONS_PER_WORKER.data(), required_argument,
       &flag, 76},
      {SHRPX_OPT_FETCH_OCSP_RESPONSE_FILE.data(), required_argument, &flag, 77},
      {SHRPX_OPT_OCSP_UPDATE_INTERVAL.data(), required_argument, &flag, 78},
      {SHRPX_OPT_NO_OCSP.data(), no_argument, &flag, 79},
      {SHRPX_OPT_HEADER_FIELD_BUFFER.data(), required_argument, &flag, 80},
      {SHRPX_OPT_MAX_HEADER_FIELDS.data(), required_argument, &flag, 81},
      {SHRPX_OPT_ADD_REQUEST_HEADER.data(), required_argument, &flag, 82},
      {SHRPX_OPT_INCLUDE.data(), required_argument, &flag, 83},
      {SHRPX_OPT_TLS_TICKET_KEY_CIPHER.data(), required_argument, &flag, 84},
      {SHRPX_OPT_HOST_REWRITE.data(), no_argument, &flag, 85},
      {SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED.data(), required_argument, &flag,
       86},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED.data(), required_argument, &flag, 87},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_INTERVAL.data(), required_argument,
       &flag, 88},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_MAX_RETRY.data(), required_argument,
       &flag, 89},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_MAX_FAIL.data(), required_argument,
       &flag, 90},
      {SHRPX_OPT_MRUBY_FILE.data(), required_argument, &flag, 91},
      {SHRPX_OPT_ACCEPT_PROXY_PROTOCOL.data(), no_argument, &flag, 93},
      {SHRPX_OPT_FASTOPEN.data(), required_argument, &flag, 94},
      {SHRPX_OPT_TLS_DYN_REC_WARMUP_THRESHOLD.data(), required_argument, &flag,
       95},
      {SHRPX_OPT_TLS_DYN_REC_IDLE_TIMEOUT.data(), required_argument, &flag, 96},
      {SHRPX_OPT_ADD_FORWARDED.data(), required_argument, &flag, 97},
      {SHRPX_OPT_STRIP_INCOMING_FORWARDED.data(), no_argument, &flag, 98},
      {SHRPX_OPT_FORWARDED_BY.data(), required_argument, &flag, 99},
      {SHRPX_OPT_FORWARDED_FOR.data(), required_argument, &flag, 100},
      {SHRPX_OPT_RESPONSE_HEADER_FIELD_BUFFER.data(), required_argument, &flag,
       101},
      {SHRPX_OPT_MAX_RESPONSE_HEADER_FIELDS.data(), required_argument, &flag,
       102},
      {SHRPX_OPT_NO_HTTP2_CIPHER_BLACK_LIST.data(), no_argument, &flag, 103},
      {SHRPX_OPT_REQUEST_HEADER_FIELD_BUFFER.data(), required_argument, &flag,
       104},
      {SHRPX_OPT_MAX_REQUEST_HEADER_FIELDS.data(), required_argument, &flag,
       105},
      {SHRPX_OPT_BACKEND_HTTP1_TLS.data(), no_argument, &flag, 106},
      {SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_TLS.data(), no_argument, &flag,
       108},
      {SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_CERT_FILE.data(),
       required_argument, &flag, 109},
      {SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_PRIVATE_KEY_FILE.data(),
       required_argument, &flag, 110},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_TLS.data(), no_argument, &flag, 111},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_CERT_FILE.data(), required_argument,
       &flag, 112},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_PRIVATE_KEY_FILE.data(),
       required_argument, &flag, 113},
      {SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_ADDRESS_FAMILY.data(),
       required_argument, &flag, 114},
      {SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_ADDRESS_FAMILY.data(),
       required_argument, &flag, 115},
      {SHRPX_OPT_BACKEND_ADDRESS_FAMILY.data(), required_argument, &flag, 116},
      {SHRPX_OPT_FRONTEND_HTTP2_MAX_CONCURRENT_STREAMS.data(),
       required_argument, &flag, 117},
      {SHRPX_OPT_BACKEND_HTTP2_MAX_CONCURRENT_STREAMS.data(), required_argument,
       &flag, 118},
      {SHRPX_OPT_BACKEND_CONNECTIONS_PER_FRONTEND.data(), required_argument,
       &flag, 119},
      {SHRPX_OPT_BACKEND_TLS.data(), no_argument, &flag, 120},
      {SHRPX_OPT_BACKEND_CONNECTIONS_PER_HOST.data(), required_argument, &flag,
       121},
      {SHRPX_OPT_ERROR_PAGE.data(), required_argument, &flag, 122},
      {SHRPX_OPT_NO_KQUEUE.data(), no_argument, &flag, 123},
      {SHRPX_OPT_FRONTEND_HTTP2_SETTINGS_TIMEOUT.data(), required_argument,
       &flag, 124},
      {SHRPX_OPT_BACKEND_HTTP2_SETTINGS_TIMEOUT.data(), required_argument,
       &flag, 125},
      {SHRPX_OPT_API_MAX_REQUEST_BODY.data(), required_argument, &flag, 126},
      {SHRPX_OPT_BACKEND_MAX_BACKOFF.data(), required_argument, &flag, 127},
      {SHRPX_OPT_SERVER_NAME.data(), required_argument, &flag, 128},
      {SHRPX_OPT_NO_SERVER_REWRITE.data(), no_argument, &flag, 129},
      {SHRPX_OPT_FRONTEND_HTTP2_OPTIMIZE_WRITE_BUFFER_SIZE.data(), no_argument,
       &flag, 130},
      {SHRPX_OPT_FRONTEND_HTTP2_OPTIMIZE_WINDOW_SIZE.data(), no_argument, &flag,
       131},
      {SHRPX_OPT_FRONTEND_HTTP2_WINDOW_SIZE.data(), required_argument, &flag,
       132},
      {SHRPX_OPT_FRONTEND_HTTP2_CONNECTION_WINDOW_SIZE.data(),
       required_argument, &flag, 133},
      {SHRPX_OPT_BACKEND_HTTP2_WINDOW_SIZE.data(), required_argument, &flag,
       134},
      {SHRPX_OPT_BACKEND_HTTP2_CONNECTION_WINDOW_SIZE.data(), required_argument,
       &flag, 135},
      {SHRPX_OPT_FRONTEND_HTTP2_ENCODER_DYNAMIC_TABLE_SIZE.data(),
       required_argument, &flag, 136},
      {SHRPX_OPT_FRONTEND_HTTP2_DECODER_DYNAMIC_TABLE_SIZE.data(),
       required_argument, &flag, 137},
      {SHRPX_OPT_BACKEND_HTTP2_ENCODER_DYNAMIC_TABLE_SIZE.data(),
       required_argument, &flag, 138},
      {SHRPX_OPT_BACKEND_HTTP2_DECODER_DYNAMIC_TABLE_SIZE.data(),
       required_argument, &flag, 139},
      {SHRPX_OPT_ECDH_CURVES.data(), required_argument, &flag, 140},
      {SHRPX_OPT_TLS_SCT_DIR.data(), required_argument, &flag, 141},
      {SHRPX_OPT_BACKEND_CONNECT_TIMEOUT.data(), required_argument, &flag, 142},
      {SHRPX_OPT_DNS_CACHE_TIMEOUT.data(), required_argument, &flag, 143},
      {SHRPX_OPT_DNS_LOOKUP_TIMEOUT.data(), required_argument, &flag, 144},
      {SHRPX_OPT_DNS_MAX_TRY.data(), required_argument, &flag, 145},
      {SHRPX_OPT_FRONTEND_KEEP_ALIVE_TIMEOUT.data(), required_argument, &flag,
       146},
      {SHRPX_OPT_PSK_SECRETS.data(), required_argument, &flag, 147},
      {SHRPX_OPT_CLIENT_PSK_SECRETS.data(), required_argument, &flag, 148},
      {SHRPX_OPT_CLIENT_NO_HTTP2_CIPHER_BLACK_LIST.data(), no_argument, &flag,
       149},
      {SHRPX_OPT_CLIENT_CIPHERS.data(), required_argument, &flag, 150},
      {SHRPX_OPT_ACCESSLOG_WRITE_EARLY.data(), no_argument, &flag, 151},
      {SHRPX_OPT_TLS_MIN_PROTO_VERSION.data(), required_argument, &flag, 152},
      {SHRPX_OPT_TLS_MAX_PROTO_VERSION.data(), required_argument, &flag, 153},
      {SHRPX_OPT_REDIRECT_HTTPS_PORT.data(), required_argument, &flag, 154},
      {SHRPX_OPT_FRONTEND_MAX_REQUESTS.data(), required_argument, &flag, 155},
      {SHRPX_OPT_SINGLE_THREAD.data(), no_argument, &flag, 156},
      {SHRPX_OPT_NO_ADD_X_FORWARDED_PROTO.data(), no_argument, &flag, 157},
      {SHRPX_OPT_NO_STRIP_INCOMING_X_FORWARDED_PROTO.data(), no_argument, &flag,
       158},
      {SHRPX_OPT_SINGLE_PROCESS.data(), no_argument, &flag, 159},
      {SHRPX_OPT_VERIFY_CLIENT_TOLERATE_EXPIRED.data(), no_argument, &flag,
       160},
      {SHRPX_OPT_IGNORE_PER_PATTERN_MRUBY_ERROR.data(), no_argument, &flag,
       161},
      {SHRPX_OPT_TLS_NO_POSTPONE_EARLY_DATA.data(), no_argument, &flag, 162},
      {SHRPX_OPT_TLS_MAX_EARLY_DATA.data(), required_argument, &flag, 163},
      {SHRPX_OPT_TLS13_CIPHERS.data(), required_argument, &flag, 164},
      {SHRPX_OPT_TLS13_CLIENT_CIPHERS.data(), required_argument, &flag, 165},
      {SHRPX_OPT_NO_STRIP_INCOMING_EARLY_DATA.data(), no_argument, &flag, 166},
      {SHRPX_OPT_NO_HTTP2_CIPHER_BLOCK_LIST.data(), no_argument, &flag, 167},
      {SHRPX_OPT_CLIENT_NO_HTTP2_CIPHER_BLOCK_LIST.data(), no_argument, &flag,
       168},
      {SHRPX_OPT_QUIC_BPF_PROGRAM_FILE.data(), required_argument, &flag, 169},
      {SHRPX_OPT_NO_QUIC_BPF.data(), no_argument, &flag, 170},
      {SHRPX_OPT_HTTP2_ALTSVC.data(), required_argument, &flag, 171},
      {SHRPX_OPT_FRONTEND_HTTP3_READ_TIMEOUT.data(), required_argument, &flag,
       172},
      {SHRPX_OPT_FRONTEND_QUIC_IDLE_TIMEOUT.data(), required_argument, &flag,
       173},
      {SHRPX_OPT_FRONTEND_QUIC_DEBUG_LOG.data(), no_argument, &flag, 174},
      {SHRPX_OPT_FRONTEND_HTTP3_WINDOW_SIZE.data(), required_argument, &flag,
       175},
      {SHRPX_OPT_FRONTEND_HTTP3_CONNECTION_WINDOW_SIZE.data(),
       required_argument, &flag, 176},
      {SHRPX_OPT_FRONTEND_HTTP3_MAX_WINDOW_SIZE.data(), required_argument,
       &flag, 177},
      {SHRPX_OPT_FRONTEND_HTTP3_MAX_CONNECTION_WINDOW_SIZE.data(),
       required_argument, &flag, 178},
      {SHRPX_OPT_FRONTEND_HTTP3_MAX_CONCURRENT_STREAMS.data(),
       required_argument, &flag, 179},
      {SHRPX_OPT_FRONTEND_QUIC_EARLY_DATA.data(), no_argument, &flag, 180},
      {SHRPX_OPT_FRONTEND_QUIC_QLOG_DIR.data(), required_argument, &flag, 181},
      {SHRPX_OPT_FRONTEND_QUIC_REQUIRE_TOKEN.data(), no_argument, &flag, 182},
      {SHRPX_OPT_FRONTEND_QUIC_CONGESTION_CONTROLLER.data(), required_argument,
       &flag, 183},
      {SHRPX_OPT_QUIC_SERVER_ID.data(), required_argument, &flag, 185},
      {SHRPX_OPT_FRONTEND_QUIC_SECRET_FILE.data(), required_argument, &flag,
       186},
      {SHRPX_OPT_RLIMIT_MEMLOCK.data(), required_argument, &flag, 187},
      {SHRPX_OPT_MAX_WORKER_PROCESSES.data(), required_argument, &flag, 188},
      {SHRPX_OPT_WORKER_PROCESS_GRACE_SHUTDOWN_PERIOD.data(), required_argument,
       &flag, 189},
      {SHRPX_OPT_FRONTEND_QUIC_INITIAL_RTT.data(), required_argument, &flag,
       190},
      {SHRPX_OPT_REQUIRE_HTTP_SCHEME.data(), no_argument, &flag, 191},
      {SHRPX_OPT_TLS_KTLS.data(), no_argument, &flag, 192},
      {SHRPX_OPT_ALPN_LIST.data(), required_argument, &flag, 193},
      {SHRPX_OPT_FRONTEND_HEADER_TIMEOUT.data(), required_argument, &flag, 194},
      {SHRPX_OPT_FRONTEND_HTTP2_IDLE_TIMEOUT.data(), required_argument, &flag,
       195},
      {SHRPX_OPT_FRONTEND_HTTP3_IDLE_TIMEOUT.data(), required_argument, &flag,
       196},
      {nullptr, 0, nullptr, 0}};

    int option_index = 0;
    int c =
      getopt_long(argc, argv, "DL:b:c:f:hkn:opsv", long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
    case 'D':
      cmdcfgs.emplace_back(SHRPX_OPT_DAEMON, "yes"_sr);
      break;
    case 'L':
      cmdcfgs.emplace_back(SHRPX_OPT_LOG_LEVEL, StringRef{optarg});
      break;
    case 'b':
      cmdcfgs.emplace_back(SHRPX_OPT_BACKEND, StringRef{optarg});
      break;
    case 'c':
      cmdcfgs.emplace_back(SHRPX_OPT_HTTP2_MAX_CONCURRENT_STREAMS,
                           StringRef{optarg});
      break;
    case 'f':
      cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND, StringRef{optarg});
      break;
    case 'h':
      print_help(std::cout);
      exit(EXIT_SUCCESS);
    case 'k':
      cmdcfgs.emplace_back(SHRPX_OPT_INSECURE, "yes"_sr);
      break;
    case 'n':
      cmdcfgs.emplace_back(SHRPX_OPT_WORKERS, StringRef{optarg});
      break;
    case 'o':
      cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_FRAME_DEBUG, "yes"_sr);
      break;
    case 'p':
      cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_PROXY, "yes"_sr);
      break;
    case 's':
      cmdcfgs.emplace_back(SHRPX_OPT_HTTP2_PROXY, "yes"_sr);
      break;
    case 'v':
      print_version(std::cout);
      exit(EXIT_SUCCESS);
    case '?':
      util::show_candidates(argv[optind - 1], long_options);
      exit(EXIT_FAILURE);
    case 0:
      switch (flag) {
      case 1:
        // --add-x-forwarded-for
        cmdcfgs.emplace_back(SHRPX_OPT_ADD_X_FORWARDED_FOR, "yes"_sr);
        break;
      case 2:
        // --frontend-http2-read-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_READ_TIMEOUT,
                             StringRef{optarg});
        break;
      case 3:
        // --frontend-read-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_READ_TIMEOUT,
                             StringRef{optarg});
        break;
      case 4:
        // --frontend-write-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_WRITE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 5:
        // --backend-read-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_READ_TIMEOUT, StringRef{optarg});
        break;
      case 6:
        // --backend-write-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_WRITE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 7:
        cmdcfgs.emplace_back(SHRPX_OPT_ACCESSLOG_FILE, StringRef{optarg});
        break;
      case 8:
        // --backend-keep-alive-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_KEEP_ALIVE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 9:
        // --frontend-http2-window-bits
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_WINDOW_BITS,
                             StringRef{optarg});
        break;
      case 10:
        cmdcfgs.emplace_back(SHRPX_OPT_PID_FILE, StringRef{optarg});
        break;
      case 11:
        cmdcfgs.emplace_back(SHRPX_OPT_USER, StringRef{optarg});
        break;
      case 12:
        // --conf
        mod_config()->conf_path =
          make_string_ref(mod_config()->balloc, StringRef{optarg});
        break;
      case 14:
        // --syslog-facility
        cmdcfgs.emplace_back(SHRPX_OPT_SYSLOG_FACILITY, StringRef{optarg});
        break;
      case 15:
        // --backlog
        cmdcfgs.emplace_back(SHRPX_OPT_BACKLOG, StringRef{optarg});
        break;
      case 16:
        // --ciphers
        cmdcfgs.emplace_back(SHRPX_OPT_CIPHERS, StringRef{optarg});
        break;
      case 17:
        // --client
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT, "yes"_sr);
        break;
      case 18:
        // --backend-http2-window-bits
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_WINDOW_BITS,
                             StringRef{optarg});
        break;
      case 19:
        // --cacert
        cmdcfgs.emplace_back(SHRPX_OPT_CACERT, StringRef{optarg});
        break;
      case 20:
        // --backend-ipv4
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_IPV4, "yes"_sr);
        break;
      case 21:
        // --backend-ipv6
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_IPV6, "yes"_sr);
        break;
      case 22:
        // --private-key-passwd-file
        cmdcfgs.emplace_back(SHRPX_OPT_PRIVATE_KEY_PASSWD_FILE,
                             StringRef{optarg});
        break;
      case 23:
        // --no-via
        cmdcfgs.emplace_back(SHRPX_OPT_NO_VIA, "yes"_sr);
        break;
      case 24:
        // --subcert
        cmdcfgs.emplace_back(SHRPX_OPT_SUBCERT, StringRef{optarg});
        break;
      case 25:
        // --http2-bridge
        cmdcfgs.emplace_back(SHRPX_OPT_HTTP2_BRIDGE, "yes"_sr);
        break;
      case 26:
        // --backend-http-proxy-uri
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP_PROXY_URI,
                             StringRef{optarg});
        break;
      case 27:
        // --backend-no-tls
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_NO_TLS, "yes"_sr);
        break;
      case 28:
        // --ocsp-startup
        cmdcfgs.emplace_back(SHRPX_OPT_OCSP_STARTUP, "yes"_sr);
        break;
      case 29:
        // --frontend-no-tls
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_NO_TLS, "yes"_sr);
        break;
      case 30:
        // --no-verify-ocsp
        cmdcfgs.emplace_back(SHRPX_OPT_NO_VERIFY_OCSP, "yes"_sr);
        break;
      case 31:
        // --backend-tls-sni-field
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_TLS_SNI_FIELD,
                             StringRef{optarg});
        break;
      case 33:
        // --dh-param-file
        cmdcfgs.emplace_back(SHRPX_OPT_DH_PARAM_FILE, StringRef{optarg});
        break;
      case 34:
        // --read-rate
        cmdcfgs.emplace_back(SHRPX_OPT_READ_RATE, StringRef{optarg});
        break;
      case 35:
        // --read-burst
        cmdcfgs.emplace_back(SHRPX_OPT_READ_BURST, StringRef{optarg});
        break;
      case 36:
        // --write-rate
        cmdcfgs.emplace_back(SHRPX_OPT_WRITE_RATE, StringRef{optarg});
        break;
      case 37:
        // --write-burst
        cmdcfgs.emplace_back(SHRPX_OPT_WRITE_BURST, StringRef{optarg});
        break;
      case 38:
        // --npn-list
        cmdcfgs.emplace_back(SHRPX_OPT_NPN_LIST, StringRef{optarg});
        break;
      case 39:
        // --verify-client
        cmdcfgs.emplace_back(SHRPX_OPT_VERIFY_CLIENT, "yes"_sr);
        break;
      case 40:
        // --verify-client-cacert
        cmdcfgs.emplace_back(SHRPX_OPT_VERIFY_CLIENT_CACERT, StringRef{optarg});
        break;
      case 41:
        // --client-private-key-file
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_PRIVATE_KEY_FILE,
                             StringRef{optarg});
        break;
      case 42:
        // --client-cert-file
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_CERT_FILE, StringRef{optarg});
        break;
      case 43:
        // --frontend-http2-dump-request-header
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_DUMP_REQUEST_HEADER,
                             StringRef{optarg});
        break;
      case 44:
        // --frontend-http2-dump-response-header
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_DUMP_RESPONSE_HEADER,
                             StringRef{optarg});
        break;
      case 45:
        // --http2-no-cookie-crumbling
        cmdcfgs.emplace_back(SHRPX_OPT_HTTP2_NO_COOKIE_CRUMBLING, "yes"_sr);
        break;
      case 46:
        // --frontend-http2-connection-window-bits
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_CONNECTION_WINDOW_BITS,
                             StringRef{optarg});
        break;
      case 47:
        // --backend-http2-connection-window-bits
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_CONNECTION_WINDOW_BITS,
                             StringRef{optarg});
        break;
      case 48:
        // --tls-proto-list
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_PROTO_LIST, StringRef{optarg});
        break;
      case 49:
        // --padding
        cmdcfgs.emplace_back(SHRPX_OPT_PADDING, StringRef{optarg});
        break;
      case 50:
        // --worker-read-rate
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_READ_RATE, StringRef{optarg});
        break;
      case 51:
        // --worker-read-burst
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_READ_BURST, StringRef{optarg});
        break;
      case 52:
        // --worker-write-rate
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_WRITE_RATE, StringRef{optarg});
        break;
      case 53:
        // --worker-write-burst
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_WRITE_BURST, StringRef{optarg});
        break;
      case 54:
        // --altsvc
        cmdcfgs.emplace_back(SHRPX_OPT_ALTSVC, StringRef{optarg});
        break;
      case 55:
        // --add-response-header
        cmdcfgs.emplace_back(SHRPX_OPT_ADD_RESPONSE_HEADER, StringRef{optarg});
        break;
      case 56:
        // --worker-frontend-connections
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_FRONTEND_CONNECTIONS,
                             StringRef{optarg});
        break;
      case 57:
        // --accesslog-syslog
        cmdcfgs.emplace_back(SHRPX_OPT_ACCESSLOG_SYSLOG, "yes"_sr);
        break;
      case 58:
        // --errorlog-file
        cmdcfgs.emplace_back(SHRPX_OPT_ERRORLOG_FILE, StringRef{optarg});
        break;
      case 59:
        // --errorlog-syslog
        cmdcfgs.emplace_back(SHRPX_OPT_ERRORLOG_SYSLOG, "yes"_sr);
        break;
      case 60:
        // --stream-read-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_STREAM_READ_TIMEOUT, StringRef{optarg});
        break;
      case 61:
        // --stream-write-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_STREAM_WRITE_TIMEOUT, StringRef{optarg});
        break;
      case 62:
        // --no-location-rewrite
        cmdcfgs.emplace_back(SHRPX_OPT_NO_LOCATION_REWRITE, "yes"_sr);
        break;
      case 63:
        // --backend-http1-connections-per-host
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP1_CONNECTIONS_PER_HOST,
                             StringRef{optarg});
        break;
      case 64:
        // --listener-disable-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_LISTENER_DISABLE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 65:
        // --strip-incoming-x-forwarded-for
        cmdcfgs.emplace_back(SHRPX_OPT_STRIP_INCOMING_X_FORWARDED_FOR,
                             "yes"_sr);
        break;
      case 66:
        // --accesslog-format
        cmdcfgs.emplace_back(SHRPX_OPT_ACCESSLOG_FORMAT, StringRef{optarg});
        break;
      case 67:
        // --backend-http1-connections-per-frontend
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP1_CONNECTIONS_PER_FRONTEND,
                             StringRef{optarg});
        break;
      case 68:
        // --tls-ticket-key-file
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_FILE, StringRef{optarg});
        break;
      case 69:
        // --rlimit-nofile
        cmdcfgs.emplace_back(SHRPX_OPT_RLIMIT_NOFILE, StringRef{optarg});
        break;
      case 71:
        // --backend-response-buffer
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_RESPONSE_BUFFER,
                             StringRef{optarg});
        break;
      case 72:
        // --backend-request-buffer
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_REQUEST_BUFFER,
                             StringRef{optarg});
        break;
      case 73:
        // --no-host-rewrite
        cmdcfgs.emplace_back(SHRPX_OPT_NO_HOST_REWRITE, "yes"_sr);
        break;
      case 74:
        // --no-server-push
        cmdcfgs.emplace_back(SHRPX_OPT_NO_SERVER_PUSH, "yes"_sr);
        break;
      case 76:
        // --backend-http2-connections-per-worker
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_CONNECTIONS_PER_WORKER,
                             StringRef{optarg});
        break;
      case 77:
        // --fetch-ocsp-response-file
        cmdcfgs.emplace_back(SHRPX_OPT_FETCH_OCSP_RESPONSE_FILE,
                             StringRef{optarg});
        break;
      case 78:
        // --ocsp-update-interval
        cmdcfgs.emplace_back(SHRPX_OPT_OCSP_UPDATE_INTERVAL, StringRef{optarg});
        break;
      case 79:
        // --no-ocsp
        cmdcfgs.emplace_back(SHRPX_OPT_NO_OCSP, "yes"_sr);
        break;
      case 80:
        // --header-field-buffer
        cmdcfgs.emplace_back(SHRPX_OPT_HEADER_FIELD_BUFFER, StringRef{optarg});
        break;
      case 81:
        // --max-header-fields
        cmdcfgs.emplace_back(SHRPX_OPT_MAX_HEADER_FIELDS, StringRef{optarg});
        break;
      case 82:
        // --add-request-header
        cmdcfgs.emplace_back(SHRPX_OPT_ADD_REQUEST_HEADER, StringRef{optarg});
        break;
      case 83:
        // --include
        cmdcfgs.emplace_back(SHRPX_OPT_INCLUDE, StringRef{optarg});
        break;
      case 84:
        // --tls-ticket-key-cipher
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_CIPHER,
                             StringRef{optarg});
        break;
      case 85:
        // --host-rewrite
        cmdcfgs.emplace_back(SHRPX_OPT_HOST_REWRITE, "yes"_sr);
        break;
      case 86:
        // --tls-session-cache-memcached
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED,
                             StringRef{optarg});
        break;
      case 87:
        // --tls-ticket-key-memcached
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED,
                             StringRef{optarg});
        break;
      case 88:
        // --tls-ticket-key-memcached-interval
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_INTERVAL,
                             StringRef{optarg});
        break;
      case 89:
        // --tls-ticket-key-memcached-max-retry
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_MAX_RETRY,
                             StringRef{optarg});
        break;
      case 90:
        // --tls-ticket-key-memcached-max-fail
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_MAX_FAIL,
                             StringRef{optarg});
        break;
      case 91:
        // --mruby-file
        cmdcfgs.emplace_back(SHRPX_OPT_MRUBY_FILE, StringRef{optarg});
        break;
      case 93:
        // --accept-proxy-protocol
        cmdcfgs.emplace_back(SHRPX_OPT_ACCEPT_PROXY_PROTOCOL, "yes"_sr);
        break;
      case 94:
        // --fastopen
        cmdcfgs.emplace_back(SHRPX_OPT_FASTOPEN, StringRef{optarg});
        break;
      case 95:
        // --tls-dyn-rec-warmup-threshold
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_DYN_REC_WARMUP_THRESHOLD,
                             StringRef{optarg});
        break;
      case 96:
        // --tls-dyn-rec-idle-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_DYN_REC_IDLE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 97:
        // --add-forwarded
        cmdcfgs.emplace_back(SHRPX_OPT_ADD_FORWARDED, StringRef{optarg});
        break;
      case 98:
        // --strip-incoming-forwarded
        cmdcfgs.emplace_back(SHRPX_OPT_STRIP_INCOMING_FORWARDED, "yes"_sr);
        break;
      case 99:
        // --forwarded-by
        cmdcfgs.emplace_back(SHRPX_OPT_FORWARDED_BY, StringRef{optarg});
        break;
      case 100:
        // --forwarded-for
        cmdcfgs.emplace_back(SHRPX_OPT_FORWARDED_FOR, StringRef{optarg});
        break;
      case 101:
        // --response-header-field-buffer
        cmdcfgs.emplace_back(SHRPX_OPT_RESPONSE_HEADER_FIELD_BUFFER,
                             StringRef{optarg});
        break;
      case 102:
        // --max-response-header-fields
        cmdcfgs.emplace_back(SHRPX_OPT_MAX_RESPONSE_HEADER_FIELDS,
                             StringRef{optarg});
        break;
      case 103:
        // --no-http2-cipher-black-list
        cmdcfgs.emplace_back(SHRPX_OPT_NO_HTTP2_CIPHER_BLACK_LIST, "yes"_sr);
        break;
      case 104:
        // --request-header-field-buffer
        cmdcfgs.emplace_back(SHRPX_OPT_REQUEST_HEADER_FIELD_BUFFER,
                             StringRef{optarg});
        break;
      case 105:
        // --max-request-header-fields
        cmdcfgs.emplace_back(SHRPX_OPT_MAX_REQUEST_HEADER_FIELDS,
                             StringRef{optarg});
        break;
      case 106:
        // --backend-http1-tls
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP1_TLS, "yes"_sr);
        break;
      case 108:
        // --tls-session-cache-memcached-tls
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_TLS,
                             "yes"_sr);
        break;
      case 109:
        // --tls-session-cache-memcached-cert-file
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_CERT_FILE,
                             StringRef{optarg});
        break;
      case 110:
        // --tls-session-cache-memcached-private-key-file
        cmdcfgs.emplace_back(
          SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_PRIVATE_KEY_FILE,
          StringRef{optarg});
        break;
      case 111:
        // --tls-ticket-key-memcached-tls
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_TLS, "yes"_sr);
        break;
      case 112:
        // --tls-ticket-key-memcached-cert-file
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_CERT_FILE,
                             StringRef{optarg});
        break;
      case 113:
        // --tls-ticket-key-memcached-private-key-file
        cmdcfgs.emplace_back(
          SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_PRIVATE_KEY_FILE,
          StringRef{optarg});
        break;
      case 114:
        // --tls-ticket-key-memcached-address-family
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_TICKET_KEY_MEMCACHED_ADDRESS_FAMILY,
                             StringRef{optarg});
        break;
      case 115:
        // --tls-session-cache-memcached-address-family
        cmdcfgs.emplace_back(
          SHRPX_OPT_TLS_SESSION_CACHE_MEMCACHED_ADDRESS_FAMILY,
          StringRef{optarg});
        break;
      case 116:
        // --backend-address-family
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_ADDRESS_FAMILY,
                             StringRef{optarg});
        break;
      case 117:
        // --frontend-http2-max-concurrent-streams
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_MAX_CONCURRENT_STREAMS,
                             StringRef{optarg});
        break;
      case 118:
        // --backend-http2-max-concurrent-streams
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_MAX_CONCURRENT_STREAMS,
                             StringRef{optarg});
        break;
      case 119:
        // --backend-connections-per-frontend
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_CONNECTIONS_PER_FRONTEND,
                             StringRef{optarg});
        break;
      case 120:
        // --backend-tls
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_TLS, "yes"_sr);
        break;
      case 121:
        // --backend-connections-per-host
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_CONNECTIONS_PER_HOST,
                             StringRef{optarg});
        break;
      case 122:
        // --error-page
        cmdcfgs.emplace_back(SHRPX_OPT_ERROR_PAGE, StringRef{optarg});
        break;
      case 123:
        // --no-kqueue
        cmdcfgs.emplace_back(SHRPX_OPT_NO_KQUEUE, "yes"_sr);
        break;
      case 124:
        // --frontend-http2-settings-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_SETTINGS_TIMEOUT,
                             StringRef{optarg});
        break;
      case 125:
        // --backend-http2-settings-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_SETTINGS_TIMEOUT,
                             StringRef{optarg});
        break;
      case 126:
        // --api-max-request-body
        cmdcfgs.emplace_back(SHRPX_OPT_API_MAX_REQUEST_BODY, StringRef{optarg});
        break;
      case 127:
        // --backend-max-backoff
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_MAX_BACKOFF, StringRef{optarg});
        break;
      case 128:
        // --server-name
        cmdcfgs.emplace_back(SHRPX_OPT_SERVER_NAME, StringRef{optarg});
        break;
      case 129:
        // --no-server-rewrite
        cmdcfgs.emplace_back(SHRPX_OPT_NO_SERVER_REWRITE, "yes"_sr);
        break;
      case 130:
        // --frontend-http2-optimize-write-buffer-size
        cmdcfgs.emplace_back(
          SHRPX_OPT_FRONTEND_HTTP2_OPTIMIZE_WRITE_BUFFER_SIZE, "yes"_sr);
        break;
      case 131:
        // --frontend-http2-optimize-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_OPTIMIZE_WINDOW_SIZE,
                             "yes"_sr);
        break;
      case 132:
        // --frontend-http2-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 133:
        // --frontend-http2-connection-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_CONNECTION_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 134:
        // --backend-http2-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 135:
        // --backend-http2-connection-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_CONNECTION_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 136:
        // --frontend-http2-encoder-dynamic-table-size
        cmdcfgs.emplace_back(
          SHRPX_OPT_FRONTEND_HTTP2_ENCODER_DYNAMIC_TABLE_SIZE,
          StringRef{optarg});
        break;
      case 137:
        // --frontend-http2-decoder-dynamic-table-size
        cmdcfgs.emplace_back(
          SHRPX_OPT_FRONTEND_HTTP2_DECODER_DYNAMIC_TABLE_SIZE,
          StringRef{optarg});
        break;
      case 138:
        // --backend-http2-encoder-dynamic-table-size
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_ENCODER_DYNAMIC_TABLE_SIZE,
                             StringRef{optarg});
        break;
      case 139:
        // --backend-http2-decoder-dynamic-table-size
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_HTTP2_DECODER_DYNAMIC_TABLE_SIZE,
                             StringRef{optarg});
        break;
      case 140:
        // --ecdh-curves
        cmdcfgs.emplace_back(SHRPX_OPT_ECDH_CURVES, StringRef{optarg});
        break;
      case 141:
        // --tls-sct-dir
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_SCT_DIR, StringRef{optarg});
        break;
      case 142:
        // --backend-connect-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_BACKEND_CONNECT_TIMEOUT,
                             StringRef{optarg});
        break;
      case 143:
        // --dns-cache-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_DNS_CACHE_TIMEOUT, StringRef{optarg});
        break;
      case 144:
        // --dns-lookup-timeou
        cmdcfgs.emplace_back(SHRPX_OPT_DNS_LOOKUP_TIMEOUT, StringRef{optarg});
        break;
      case 145:
        // --dns-max-try
        cmdcfgs.emplace_back(SHRPX_OPT_DNS_MAX_TRY, StringRef{optarg});
        break;
      case 146:
        // --frontend-keep-alive-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_KEEP_ALIVE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 147:
        // --psk-secrets
        cmdcfgs.emplace_back(SHRPX_OPT_PSK_SECRETS, StringRef{optarg});
        break;
      case 148:
        // --client-psk-secrets
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_PSK_SECRETS, StringRef{optarg});
        break;
      case 149:
        // --client-no-http2-cipher-black-list
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_NO_HTTP2_CIPHER_BLACK_LIST,
                             "yes"_sr);
        break;
      case 150:
        // --client-ciphers
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_CIPHERS, StringRef{optarg});
        break;
      case 151:
        // --accesslog-write-early
        cmdcfgs.emplace_back(SHRPX_OPT_ACCESSLOG_WRITE_EARLY, "yes"_sr);
        break;
      case 152:
        // --tls-min-proto-version
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_MIN_PROTO_VERSION,
                             StringRef{optarg});
        break;
      case 153:
        // --tls-max-proto-version
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_MAX_PROTO_VERSION,
                             StringRef{optarg});
        break;
      case 154:
        // --redirect-https-port
        cmdcfgs.emplace_back(SHRPX_OPT_REDIRECT_HTTPS_PORT, StringRef{optarg});
        break;
      case 155:
        // --frontend-max-requests
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_MAX_REQUESTS,
                             StringRef{optarg});
        break;
      case 156:
        // --single-thread
        cmdcfgs.emplace_back(SHRPX_OPT_SINGLE_THREAD, "yes"_sr);
        break;
      case 157:
        // --no-add-x-forwarded-proto
        cmdcfgs.emplace_back(SHRPX_OPT_NO_ADD_X_FORWARDED_PROTO, "yes"_sr);
        break;
      case 158:
        // --no-strip-incoming-x-forwarded-proto
        cmdcfgs.emplace_back(SHRPX_OPT_NO_STRIP_INCOMING_X_FORWARDED_PROTO,
                             "yes"_sr);
        break;
      case 159:
        // --single-process
        cmdcfgs.emplace_back(SHRPX_OPT_SINGLE_PROCESS, "yes"_sr);
        break;
      case 160:
        // --verify-client-tolerate-expired
        cmdcfgs.emplace_back(SHRPX_OPT_VERIFY_CLIENT_TOLERATE_EXPIRED,
                             "yes"_sr);
        break;
      case 161:
        // --ignore-per-pattern-mruby-error
        cmdcfgs.emplace_back(SHRPX_OPT_IGNORE_PER_PATTERN_MRUBY_ERROR,
                             "yes"_sr);
        break;
      case 162:
        // --tls-no-postpone-early-data
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_NO_POSTPONE_EARLY_DATA, "yes"_sr);
        break;
      case 163:
        // --tls-max-early-data
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_MAX_EARLY_DATA, StringRef{optarg});
        break;
      case 164:
        // --tls13-ciphers
        cmdcfgs.emplace_back(SHRPX_OPT_TLS13_CIPHERS, StringRef{optarg});
        break;
      case 165:
        // --tls13-client-ciphers
        cmdcfgs.emplace_back(SHRPX_OPT_TLS13_CLIENT_CIPHERS, StringRef{optarg});
        break;
      case 166:
        // --no-strip-incoming-early-data
        cmdcfgs.emplace_back(SHRPX_OPT_NO_STRIP_INCOMING_EARLY_DATA, "yes"_sr);
        break;
      case 167:
        // --no-http2-cipher-block-list
        cmdcfgs.emplace_back(SHRPX_OPT_NO_HTTP2_CIPHER_BLOCK_LIST, "yes"_sr);
        break;
      case 168:
        // --client-no-http2-cipher-block-list
        cmdcfgs.emplace_back(SHRPX_OPT_CLIENT_NO_HTTP2_CIPHER_BLOCK_LIST,
                             "yes"_sr);
        break;
      case 169:
        // --quic-bpf-program-file
        cmdcfgs.emplace_back(SHRPX_OPT_QUIC_BPF_PROGRAM_FILE,
                             StringRef{optarg});
        break;
      case 170:
        // --no-quic-bpf
        cmdcfgs.emplace_back(SHRPX_OPT_NO_QUIC_BPF, "yes"_sr);
        break;
      case 171:
        // --http2-altsvc
        cmdcfgs.emplace_back(SHRPX_OPT_HTTP2_ALTSVC, StringRef{optarg});
        break;
      case 172:
        // --frontend-http3-read-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_READ_TIMEOUT,
                             StringRef{optarg});
        break;
      case 173:
        // --frontend-quic-idle-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_IDLE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 174:
        // --frontend-quic-debug-log
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_DEBUG_LOG, "yes"_sr);
        break;
      case 175:
        // --frontend-http3-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 176:
        // --frontend-http3-connection-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_CONNECTION_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 177:
        // --frontend-http3-max-window-size
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_MAX_WINDOW_SIZE,
                             StringRef{optarg});
        break;
      case 178:
        // --frontend-http3-max-connection-window-size
        cmdcfgs.emplace_back(
          SHRPX_OPT_FRONTEND_HTTP3_MAX_CONNECTION_WINDOW_SIZE,
          StringRef{optarg});
        break;
      case 179:
        // --frontend-http3-max-concurrent-streams
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_MAX_CONCURRENT_STREAMS,
                             StringRef{optarg});
        break;
      case 180:
        // --frontend-quic-early-data
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_EARLY_DATA, "yes"_sr);
        break;
      case 181:
        // --frontend-quic-qlog-dir
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_QLOG_DIR,
                             StringRef{optarg});
        break;
      case 182:
        // --frontend-quic-require-token
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_REQUIRE_TOKEN, "yes"_sr);
        break;
      case 183:
        // --frontend-quic-congestion-controller
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_CONGESTION_CONTROLLER,
                             StringRef{optarg});
        break;
      case 185:
        // --quic-server-id
        cmdcfgs.emplace_back(SHRPX_OPT_QUIC_SERVER_ID, StringRef{optarg});
        break;
      case 186:
        // --frontend-quic-secret-file
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_SECRET_FILE,
                             StringRef{optarg});
        break;
      case 187:
        // --rlimit-memlock
        cmdcfgs.emplace_back(SHRPX_OPT_RLIMIT_MEMLOCK, StringRef{optarg});
        break;
      case 188:
        // --max-worker-processes
        cmdcfgs.emplace_back(SHRPX_OPT_MAX_WORKER_PROCESSES, StringRef{optarg});
        break;
      case 189:
        // --worker-process-grace-shutdown-period
        cmdcfgs.emplace_back(SHRPX_OPT_WORKER_PROCESS_GRACE_SHUTDOWN_PERIOD,
                             StringRef{optarg});
        break;
      case 190:
        // --frontend-quic-initial-rtt
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_QUIC_INITIAL_RTT,
                             StringRef{optarg});
        break;
      case 191:
        // --require-http-scheme
        cmdcfgs.emplace_back(SHRPX_OPT_REQUIRE_HTTP_SCHEME, "yes"_sr);
        break;
      case 192:
        // --tls-ktls
        cmdcfgs.emplace_back(SHRPX_OPT_TLS_KTLS, "yes"_sr);
        break;
      case 193:
        // --alpn-list
        cmdcfgs.emplace_back(SHRPX_OPT_ALPN_LIST, StringRef{optarg});
        break;
      case 194:
        // --frontend-header-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HEADER_TIMEOUT,
                             StringRef{optarg});
        break;
      case 195:
        // --frontend-http2-idle-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP2_IDLE_TIMEOUT,
                             StringRef{optarg});
        break;
      case 196:
        // --frontend-http3-idle-timeout
        cmdcfgs.emplace_back(SHRPX_OPT_FRONTEND_HTTP3_IDLE_TIMEOUT,
                             StringRef{optarg});
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
  }

  if (argc - optind >= 2) {
    cmdcfgs.emplace_back(SHRPX_OPT_PRIVATE_KEY_FILE, StringRef{argv[optind++]});
    cmdcfgs.emplace_back(SHRPX_OPT_CERTIFICATE_FILE, StringRef{argv[optind++]});
  }

  rv = process_options(mod_config(), cmdcfgs);
  if (rv != 0) {
    return -1;
  }

  if (event_loop() != 0) {
    return -1;
  }

  LOG(NOTICE) << "Shutdown momentarily";

  delete_log_config();

  return 0;
}

} // namespace shrpx

int main(int argc, char **argv) { return run_app(shrpx::main, argc, argv); }
