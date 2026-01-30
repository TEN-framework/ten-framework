/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#include "shrpx_accept_handler.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H

#include <cerrno>

#include "shrpx_connection_handler.h"
#include "shrpx_config.h"
#include "shrpx_log.h"
#include "util.h"

using namespace nghttp2;

namespace shrpx {

namespace {
void acceptcb(struct ev_loop *loop, ev_io *w, int revent) {
  auto h = static_cast<AcceptHandler *>(w->data);
  h->accept_connection();
}
} // namespace

AcceptHandler::AcceptHandler(const UpstreamAddr *faddr, ConnectionHandler *h)
  : conn_hnr_(h), faddr_(faddr) {
  ev_io_init(&wev_, acceptcb, faddr_->fd, EV_READ);
  wev_.data = this;
  ev_io_start(conn_hnr_->get_loop(), &wev_);
}

AcceptHandler::~AcceptHandler() {
  ev_io_stop(conn_hnr_->get_loop(), &wev_);
  close(faddr_->fd);
}

void AcceptHandler::accept_connection() {
  sockaddr_union sockaddr;
  socklen_t addrlen = sizeof(sockaddr);

#ifdef HAVE_ACCEPT4
  auto cfd =
    accept4(faddr_->fd, &sockaddr.sa, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else  // !HAVE_ACCEPT4
  auto cfd = accept(faddr_->fd, &sockaddr.sa, &addrlen);
#endif // !HAVE_ACCEPT4

  if (cfd == -1) {
    switch (errno) {
    case EINTR:
    case ENETDOWN:
    case EPROTO:
    case ENOPROTOOPT:
    case EHOSTDOWN:
#ifdef ENONET
    case ENONET:
#endif // ENONET
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
      return;
    case EMFILE:
    case ENFILE:
      LOG(WARN) << "acceptor: running out file descriptor; disable acceptor "
                   "temporarily";
      conn_hnr_->sleep_acceptor(get_config()->conn.listener.timeout.sleep);
      return;
    default:
      return;
    }
  }

#ifndef HAVE_ACCEPT4
  util::make_socket_nonblocking(cfd);
  util::make_socket_closeonexec(cfd);
#endif // !HAVE_ACCEPT4

  conn_hnr_->handle_connection(cfd, &sockaddr.sa, addrlen, faddr_);
}

void AcceptHandler::enable() { ev_io_start(conn_hnr_->get_loop(), &wev_); }

void AcceptHandler::disable() { ev_io_stop(conn_hnr_->get_loop(), &wev_); }

int AcceptHandler::get_fd() const { return faddr_->fd; }

} // namespace shrpx
