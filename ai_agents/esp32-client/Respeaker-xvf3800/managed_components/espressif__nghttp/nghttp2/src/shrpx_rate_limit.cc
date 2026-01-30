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
#include "shrpx_rate_limit.h"

#include <limits>

#include "shrpx_connection.h"
#include "shrpx_log.h"

namespace shrpx {

namespace {
void regencb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto r = static_cast<RateLimit *>(w->data);
  r->regen();
}
} // namespace

RateLimit::RateLimit(struct ev_loop *loop, ev_io *w, size_t rate, size_t burst,
                     Connection *conn)
  : w_(w),
    loop_(loop),
    conn_(conn),
    rate_(rate),
    burst_(burst),
    avail_(burst),
    startw_req_(false) {
  ev_timer_init(&t_, regencb, 0., 1.);
  t_.data = this;
  if (rate_ > 0) {
    ev_timer_again(loop_, &t_);
  }
}

RateLimit::~RateLimit() { ev_timer_stop(loop_, &t_); }

size_t RateLimit::avail() const {
  if (rate_ == 0) {
    return std::numeric_limits<ssize_t>::max();
  }
  return avail_;
}

void RateLimit::drain(size_t n) {
  if (rate_ == 0) {
    return;
  }
  n = std::min(avail_, n);
  avail_ -= n;
  if (avail_ == 0) {
    ev_io_stop(loop_, w_);
  }
}

void RateLimit::regen() {
  if (rate_ == 0) {
    return;
  }
  if (avail_ + rate_ > burst_) {
    avail_ = burst_;
  } else {
    avail_ += rate_;
  }

  if (w_->fd >= 0 && avail_ > 0 && startw_req_) {
    ev_io_start(loop_, w_);
    handle_tls_pending_read();
  }
}

void RateLimit::startw() {
  if (w_->fd < 0) {
    return;
  }
  startw_req_ = true;
  if (rate_ == 0 || avail_ > 0) {
    ev_io_start(loop_, w_);
    handle_tls_pending_read();
    return;
  }
}

void RateLimit::stopw() {
  startw_req_ = false;
  ev_io_stop(loop_, w_);
}

void RateLimit::handle_tls_pending_read() {
  if (!conn_ || !conn_->tls.ssl ||
      (SSL_pending(conn_->tls.ssl) == 0 && conn_->tls.rbuf.rleft() == 0 &&
       (!conn_->tls.initial_handshake_done ||
        conn_->tls.earlybuf.rleft() == 0))) {
    return;
  }

  // Note that ev_feed_event works without starting watcher, but we
  // only call this function if watcher is active.
  ev_feed_event(loop_, w_, EV_READ);
}

} // namespace shrpx
