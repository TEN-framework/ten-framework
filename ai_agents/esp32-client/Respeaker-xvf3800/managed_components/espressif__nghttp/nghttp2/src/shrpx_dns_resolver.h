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
#ifndef SHRPX_DNS_RESOLVER_H
#define SHRPX_DNS_RESOLVER_H

#include "shrpx.h"

#include <sys/socket.h>
#include <netinet/in.h>

#include <vector>

#include <ev.h>
#include <ares.h>

#include "template.h"
#include "network.h"

using namespace nghttp2;

namespace shrpx {

enum class DNSResolverStatus {
  // Resolver is in initial status
  IDLE,
  // Resolver is currently resolving host name
  RUNNING,
  // Resolver successfully resolved host name
  OK,
  // Resolver failed to resolve host name
  ERROR,
};

// Callback function called when host name lookup is finished.
// |status| is either DNSResolverStatus::OK, or
// DNSResolverStatus::ERROR.  If |status| is DNSResolverStatus::OK,
// |result| points to the resolved address.  Note that port portion of
// |result| is undefined, and must be initialized by application.
// This callback function is not called if name lookup finishes in
// DNSResolver::resolve() completely.  In this case, application
// should call DNSResolver::get_status() to get current status and
// result.  In other words, callback is called if get_status() returns
// DNSResolverStatus::RUNNING.
using CompleteCb =
  std::function<void(DNSResolverStatus status, const Address *result)>;

// DNSResolver is asynchronous name resolver, backed by c-ares
// library.
class DNSResolver {
public:
  DNSResolver(struct ev_loop *loop);
  ~DNSResolver();

  // Starts resolving hostname |name|.
  int resolve(const StringRef &name, int family);
  // Returns status.  If status_ is DNSResolverStatus::SUCCESS &&
  // |result| is not nullptr, |*result| is filled.
  DNSResolverStatus get_status(Address *result) const;
  // Sets callback function when name lookup finishes.  The callback
  // function is called in a way that it can destroy this DNSResolver.
  void set_complete_cb(CompleteCb cb);
  CompleteCb get_complete_cb() const;

  // Calls these functions when read/write event occurred respectively.
  int on_read(int fd);
  int on_write(int fd);
  int on_timeout();
  // Calls this function when DNS query finished.
  void on_result(int status, ares_addrinfo *result);
  void reset_timeout();

  void start_rev(int fd);
  void stop_rev(int fd);
  void start_wev(int fd);
  void stop_wev(int fd);

private:
  int handle_event(int rfd, int wfd);

  std::vector<std::unique_ptr<ev_io>> revs_, wevs_;
  Address result_;
  CompleteCb completeCb_;
  ev_timer timer_;
  StringRef name_;
  struct ev_loop *loop_;
  // ares_channel is pointer type
  ares_channel channel_;
  // AF_INET or AF_INET6.  AF_INET for A record lookup, and AF_INET6
  // for AAAA record lookup.
  int family_;
  DNSResolverStatus status_;
};

} // namespace shrpx

#endif // SHRPX_DNS_RESOLVER_H
