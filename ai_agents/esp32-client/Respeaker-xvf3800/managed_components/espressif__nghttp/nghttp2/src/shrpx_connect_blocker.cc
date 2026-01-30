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
#include "shrpx_connect_blocker.h"
#include "shrpx_config.h"
#include "shrpx_log.h"

namespace shrpx {

namespace {
void connect_blocker_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  auto connect_blocker = static_cast<ConnectBlocker *>(w->data);
  if (LOG_ENABLED(INFO)) {
    LOG(INFO) << "Unblock";
  }

  connect_blocker->call_unblock_func();
}
} // namespace

ConnectBlocker::ConnectBlocker(std::mt19937 &gen, struct ev_loop *loop,
                               std::function<void()> block_func,
                               std::function<void()> unblock_func)
  : gen_(gen),
    block_func_(std::move(block_func)),
    unblock_func_(std::move(unblock_func)),
    loop_(loop),
    fail_count_(0),
    offline_(false) {
  ev_timer_init(&timer_, connect_blocker_cb, 0., 0.);
  timer_.data = this;
}

ConnectBlocker::~ConnectBlocker() { ev_timer_stop(loop_, &timer_); }

bool ConnectBlocker::blocked() const { return ev_is_active(&timer_); }

void ConnectBlocker::on_success() {
  if (ev_is_active(&timer_)) {
    return;
  }

  fail_count_ = 0;
}

// Use the similar backoff algorithm described in
// https://github.com/grpc/grpc/blob/master/doc/connection-backoff.md
namespace {
constexpr size_t MAX_BACKOFF_EXP = 10;
constexpr auto MULTIPLIER = 1.6;
constexpr auto JITTER = 0.2;
} // namespace

void ConnectBlocker::on_failure() {
  if (ev_is_active(&timer_)) {
    return;
  }

  call_block_func();

  ++fail_count_;

  auto base_backoff =
    util::int_pow(MULTIPLIER, std::min(MAX_BACKOFF_EXP, fail_count_));
  auto dist = std::uniform_real_distribution<>(-JITTER * base_backoff,
                                               JITTER * base_backoff);

  auto &downstreamconf = *get_config()->conn.downstream;

  auto backoff =
    std::min(downstreamconf.timeout.max_backoff, base_backoff + dist(gen_));

  LOG(WARN) << "Could not connect " << fail_count_
            << " times in a row; sleep for " << backoff << " seconds";

  ev_timer_set(&timer_, backoff, 0.);
  ev_timer_start(loop_, &timer_);
}

size_t ConnectBlocker::get_fail_count() const { return fail_count_; }

void ConnectBlocker::offline() {
  if (offline_) {
    return;
  }

  if (!ev_is_active(&timer_)) {
    call_block_func();
  }

  offline_ = true;

  ev_timer_stop(loop_, &timer_);
  ev_timer_set(&timer_, std::numeric_limits<double>::max(), 0.);
  ev_timer_start(loop_, &timer_);
}

void ConnectBlocker::online() {
  ev_timer_stop(loop_, &timer_);

  call_unblock_func();

  fail_count_ = 0;

  offline_ = false;
}

bool ConnectBlocker::in_offline() const { return offline_; }

void ConnectBlocker::call_block_func() {
  if (block_func_) {
    block_func_();
  }
}

void ConnectBlocker::call_unblock_func() {
  if (unblock_func_) {
    unblock_func_();
  }
}

} // namespace shrpx
