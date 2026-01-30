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
#ifndef SHRPX_DNS_TRACKER_H
#define SHRPX_DNS_TRACKER_H

#include "shrpx.h"

#include <map>
#include <chrono>

#include "shrpx_dual_dns_resolver.h"

using namespace nghttp2;

namespace shrpx {

struct DNSQuery {
  DNSQuery(StringRef host, CompleteCb cb)
    : host(std::move(host)),
      cb(std::move(cb)),
      dlnext(nullptr),
      dlprev(nullptr),
      status(DNSResolverStatus::IDLE),
      in_qlist(false) {}

  // Host name we lookup for.
  StringRef host;
  // Callback function called when name lookup finished.  This
  // callback is not called if name lookup finishes within
  // DNSTracker::resolve().
  CompleteCb cb;
  DNSQuery *dlnext, *dlprev;
  DNSResolverStatus status;
  // true if this object is in linked list ResolverEntry::qlist.
  bool in_qlist;
};

struct ResolverEntry {
  // Host name this entry lookups for.
  ImmutableString host;
  // DNS resolver.  Only non-nullptr if status is
  // DNSResolverStatus::RUNNING.
  std::unique_ptr<DualDNSResolver> resolv;
  // DNSQuery interested in this name lookup result.  The result is
  // notified to them all.
  DList<DNSQuery> qlist;
  // Use the same enum with DNSResolverStatus
  DNSResolverStatus status;
  // result and its expiry time
  Address result;
  // time point when cached result expires.
  std::chrono::steady_clock::time_point expiry;
};

class DNSTracker {
public:
  DNSTracker(struct ev_loop *loop, int family);
  ~DNSTracker();

  // Lookups host name described in |dnsq|.  If name lookup finishes
  // within this function (either it came from /etc/hosts, host name
  // is numeric, lookup result is cached, etc), it returns
  // DNSResolverStatus::OK or DNSResolverStatus::ERROR.  If lookup is
  // successful, DNSResolverStatus::OK is returned, and |result| is
  // filled.  If lookup failed, DNSResolverStatus::ERROR is returned.
  // If name lookup is being done background, it returns
  // DNSResolverStatus::RUNNING.  Its completion is notified by
  // calling dnsq->cb.
  DNSResolverStatus resolve(Address *result, DNSQuery *dnsq);
  // Cancels name lookup requested by |dnsq|.
  void cancel(DNSQuery *dnsq);
  // Removes expired entries from ents_.
  void gc();
  // Starts GC timer.
  void start_gc_timer();

private:
  ResolverEntry make_entry(std::unique_ptr<DualDNSResolver> resolv,
                           ImmutableString host, DNSResolverStatus status,
                           const Address *result);

  void update_entry(ResolverEntry &ent, std::unique_ptr<DualDNSResolver> resolv,
                    DNSResolverStatus status, const Address *result);

  void add_to_qlist(ResolverEntry &ent, DNSQuery *dnsq);

  std::map<StringRef, ResolverEntry> ents_;
  // Periodically iterates ents_, and removes expired entries to avoid
  // excessive use of memory.  Since only backend API can potentially
  // increase memory consumption, interval could be very long.
  ev_timer gc_timer_;
  struct ev_loop *loop_;
  // IP version preference.
  int family_;
};

} // namespace shrpx

#endif // SHRPX_DNS_TRACKER_H
