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
#ifndef SHRPX_WORKER_PROCESS_H
#define SHRPX_WORKER_PROCESS_H

#include "shrpx.h"

#include <vector>
#include <array>

#include "shrpx_connection_handler.h"
#ifdef ENABLE_HTTP3
#  include "shrpx_quic.h"
#endif // ENABLE_HTTP3

namespace shrpx {

class ConnectionHandler;

struct WorkerProcessConfig {
  // IPC socket to read event from main process
  int ipc_fd;
  // IPC socket to tell that a worker process is ready for service.
  int ready_ipc_fd;
  // IPv4 or UNIX domain socket, or -1 if not used
  int server_fd;
  // IPv6 socket, or -1 if not used
  int server_fd6;
#ifdef ENABLE_HTTP3
  // Worker IDs for the new worker process.
  std::vector<WorkerID> worker_ids;
  // IPC socket to read forwarded QUIC UDP datagram from the current
  // worker process.
  int quic_ipc_fd;
  // Lingering worker processes which were created before this worker
  // process to forward QUIC UDP datagram during reload.
  std::vector<QUICLingeringWorkerProcess> quic_lingering_worker_processes;
#endif // ENABLE_HTTP3
};

int worker_process_event_loop(WorkerProcessConfig *wpconf);

} // namespace shrpx

#endif // SHRPX_WORKER_PROCESS_H
