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
#include "shrpx_dns_resolver.h"

#include <cstring>
#include <sys/time.h>

#include "shrpx_log.h"
#include "shrpx_connection.h"
#include "shrpx_config.h"

namespace shrpx {

namespace {
void sock_state_cb(void *data, int s, int read, int write) {
  auto resolv = static_cast<DNSResolver *>(data);

  if (resolv->get_status(nullptr) != DNSResolverStatus::RUNNING) {
    return;
  }

  if (read) {
    resolv->start_rev(s);
  } else {
    resolv->stop_rev(s);
  }
  if (write) {
    resolv->start_wev(s);
  } else {
    resolv->stop_wev(s);
  }
}
} // namespace

namespace {
void addrinfo_cb(void *arg, int status, int timeouts, ares_addrinfo *result) {
  auto resolv = static_cast<DNSResolver *>(arg);
  resolv->on_result(status, result);

  ares_freeaddrinfo(result);
}
} // namespace

namespace {
void process_result(DNSResolver *resolv) {
  auto cb = resolv->get_complete_cb();
  if (!cb) {
    return;
  }
  Address result;
  auto status = resolv->get_status(&result);
  switch (status) {
  case DNSResolverStatus::OK:
  case DNSResolverStatus::ERROR:
    cb(status, &result);
    break;
  default:
    break;
  }
  // resolv may be deleted here.
}
} // namespace

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revents) {
  auto resolv = static_cast<DNSResolver *>(w->data);
  resolv->on_read(w->fd);
  process_result(resolv);
}
} // namespace

namespace {
void writecb(struct ev_loop *loop, ev_io *w, int revents) {
  auto resolv = static_cast<DNSResolver *>(w->data);
  resolv->on_write(w->fd);
  process_result(resolv);
}
} // namespace

namespace {
void timeoutcb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto resolv = static_cast<DNSResolver *>(w->data);
  resolv->on_timeout();
  process_result(resolv);
}
} // namespace

namespace {
void stop_ev(struct ev_loop *loop,
             const std::vector<std::unique_ptr<ev_io>> &evs) {
  for (auto &w : evs) {
    ev_io_stop(loop, w.get());
  }
}
} // namespace

DNSResolver::DNSResolver(struct ev_loop *loop)
  : result_{},
    loop_(loop),
    channel_(nullptr),
    family_(AF_UNSPEC),
    status_(DNSResolverStatus::IDLE) {
  ev_timer_init(&timer_, timeoutcb, 0., 0.);
  timer_.data = this;
}

DNSResolver::~DNSResolver() {
  if (channel_) {
    ares_destroy(channel_);
  }

  stop_ev(loop_, revs_);
  stop_ev(loop_, wevs_);

  ev_timer_stop(loop_, &timer_);
}

int DNSResolver::resolve(const StringRef &name, int family) {
  if (status_ != DNSResolverStatus::IDLE) {
    return -1;
  }

  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Start resolving host " << name << " in IPv"
              << (family == AF_INET ? "4" : "6");
  }

  name_ = name;
  family_ = family;

  int rv;

  auto &dnsconf = get_config()->dns;

  ares_options opts{};
  opts.sock_state_cb = sock_state_cb;
  opts.sock_state_cb_data = this;
  opts.timeout = static_cast<int>(dnsconf.timeout.lookup * 1000);
  opts.tries = dnsconf.max_try;

  auto optmask = ARES_OPT_SOCK_STATE_CB | ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES;

  ares_channel chan;
  rv = ares_init_options(&chan, &opts, optmask);
  if (rv != ARES_SUCCESS) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "ares_init_options failed: " << ares_strerror(rv);
    }
    status_ = DNSResolverStatus::ERROR;
    return -1;
  }

  channel_ = chan;
  status_ = DNSResolverStatus::RUNNING;

  ares_addrinfo_hints hints{};
  hints.ai_family = family_;

  ares_getaddrinfo(channel_, name_.data(), nullptr, &hints, addrinfo_cb, this);
  reset_timeout();

  return 0;
}

int DNSResolver::on_read(int fd) { return handle_event(fd, ARES_SOCKET_BAD); }

int DNSResolver::on_write(int fd) { return handle_event(ARES_SOCKET_BAD, fd); }

int DNSResolver::on_timeout() {
  return handle_event(ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}

int DNSResolver::handle_event(int rfd, int wfd) {
  if (status_ == DNSResolverStatus::IDLE) {
    return -1;
  }

  ares_process_fd(channel_, rfd, wfd);

  switch (status_) {
  case DNSResolverStatus::RUNNING:
    reset_timeout();
    return 0;
  case DNSResolverStatus::OK:
    return 0;
  case DNSResolverStatus::ERROR:
    return -1;
  default:
    // Unreachable
    assert(0);
    abort();
  }
}

void DNSResolver::reset_timeout() {
  if (status_ != DNSResolverStatus::RUNNING) {
    return;
  }
  timeval tvout;
  auto tv = ares_timeout(channel_, nullptr, &tvout);
  if (tv == nullptr) {
    return;
  }
  // To avoid that timer_.repeat becomes 0, which makes ev_timer_again
  // useless, add tiny fraction of time.
  timer_.repeat = tv->tv_sec + tv->tv_usec / 1000000. + 1e-9;
  ev_timer_again(loop_, &timer_);
}

DNSResolverStatus DNSResolver::get_status(Address *result) const {
  if (status_ != DNSResolverStatus::OK) {
    return status_;
  }

  if (result) {
    memcpy(result, &result_, sizeof(result_));
  }

  return status_;
}

namespace {
void start_ev(std::vector<std::unique_ptr<ev_io>> &evs, struct ev_loop *loop,
              int fd, int event, IOCb cb, void *data) {
  for (auto &w : evs) {
    if (w->fd == fd) {
      return;
    }
  }
  for (auto &w : evs) {
    if (w->fd == -1) {
      ev_io_set(w.get(), fd, event);
      ev_io_start(loop, w.get());
      return;
    }
  }

  auto w = std::make_unique<ev_io>();
  ev_io_init(w.get(), cb, fd, event);
  w->data = data;
  ev_io_start(loop, w.get());
  evs.emplace_back(std::move(w));
}
} // namespace

namespace {
void stop_ev(std::vector<std::unique_ptr<ev_io>> &evs, struct ev_loop *loop,
             int fd, int event) {
  for (auto &w : evs) {
    if (w->fd == fd) {
      ev_io_stop(loop, w.get());
      ev_io_set(w.get(), -1, event);
      return;
    }
  }
}
} // namespace

void DNSResolver::start_rev(int fd) {
  start_ev(revs_, loop_, fd, EV_READ, readcb, this);
}

void DNSResolver::stop_rev(int fd) { stop_ev(revs_, loop_, fd, EV_READ); }

void DNSResolver::start_wev(int fd) {
  start_ev(wevs_, loop_, fd, EV_WRITE, writecb, this);
}

void DNSResolver::stop_wev(int fd) { stop_ev(wevs_, loop_, fd, EV_WRITE); }

void DNSResolver::on_result(int status, ares_addrinfo *ai) {
  stop_ev(loop_, revs_);
  stop_ev(loop_, wevs_);
  ev_timer_stop(loop_, &timer_);

  if (status != ARES_SUCCESS) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Name lookup for " << name_
                << " failed: " << ares_strerror(status);
    }
    status_ = DNSResolverStatus::ERROR;
    return;
  }

  auto ap = ai->nodes;

  for (; ap; ap = ap->ai_next) {
    switch (ap->ai_family) {
    case AF_INET:
      status_ = DNSResolverStatus::OK;
      result_.len = sizeof(result_.su.in);

      assert(sizeof(result_.su.in) == ap->ai_addrlen);

      memcpy(&result_.su.in, ap->ai_addr, sizeof(result_.su.in));

      break;
    case AF_INET6:
      status_ = DNSResolverStatus::OK;
      result_.len = sizeof(result_.su.in6);

      assert(sizeof(result_.su.in6) == ap->ai_addrlen);

      memcpy(&result_.su.in6, ap->ai_addr, sizeof(result_.su.in6));

      break;
    default:
      continue;
    }

    break;
  }

  if (!ap) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Name lookup for " << name_
                << " failed: no address returned";
    }
    status_ = DNSResolverStatus::ERROR;
    return;
  }

  if (status_ == DNSResolverStatus::OK) {
    if (LOG_ENABLED(INFO)) {
      LOG(INFO) << "Name lookup succeeded: " << name_ << " -> "
                << util::numeric_name(&result_.su.sa, result_.len);
    }
    return;
  }

  status_ = DNSResolverStatus::ERROR;
}

void DNSResolver::set_complete_cb(CompleteCb cb) {
  completeCb_ = std::move(cb);
}

CompleteCb DNSResolver::get_complete_cb() const { return completeCb_; }

} // namespace shrpx
