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
#ifndef SHRPX_CONNECT_BLOCKER_H
#define SHRPX_CONNECT_BLOCKER_H

#include "shrpx.h"

#include <random>
#include <functional>

#include <ev.h>

namespace shrpx {

class ConnectBlocker {
public:
  ConnectBlocker(std::mt19937 &gen, struct ev_loop *loop,
                 std::function<void()> block_func,
                 std::function<void()> unblock_func);
  ~ConnectBlocker();

  // Returns true if making connection is not allowed.
  bool blocked() const;
  // Call this function if connect operation succeeded.  This will
  // reset sleep_ to minimum value.
  void on_success();
  // Call this function if connect operations failed.  This will start
  // timer and blocks connection establishment with exponential
  // backoff.
  void on_failure();

  size_t get_fail_count() const;

  // Peer is now considered offline.  This effectively means that the
  // connection is blocked until online() is called.
  void offline();

  // Peer is now considered online
  void online();

  // Returns true if peer is considered offline.
  bool in_offline() const;

  void call_block_func();
  void call_unblock_func();

private:
  std::mt19937 &gen_;
  // Called when blocking is started
  std::function<void()> block_func_;
  // Called when unblocked
  std::function<void()> unblock_func_;
  ev_timer timer_;
  struct ev_loop *loop_;
  // The number of consecutive connection failure.  Reset to 0 on
  // success.
  size_t fail_count_;
  // true if peer is considered offline.
  bool offline_;
};

} // namespace shrpx

#endif // SHRPX_CONNECT_BLOCKER_H
