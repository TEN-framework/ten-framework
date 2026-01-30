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
#ifndef SHRPX_DUAL_DNS_RESOLVER_H
#define SHRPX_DUAL_DNS_RESOLVER_H

#include "shrpx.h"

#include <ev.h>

#include "shrpx_dns_resolver.h"

using namespace nghttp2;

namespace shrpx {

// DualDNSResolver performs name resolution for both A and AAAA
// records at the same time.  The first successful return (or if we
// have both successful results, prefer to AAAA) is chosen.  This is
// wrapper around 2 DNSResolver inside.  resolve(), get_status(), and
// how CompleteCb is called have the same semantics with DNSResolver.
class DualDNSResolver {
public:
  // |family| controls IP version preference.  If |family| ==
  // AF_UNSPEC, bot A and AAAA lookups are performed.  If |family| ==
  // AF_INET, only A lookup is performed.  If |family| == AF_INET6,
  // only AAAA lookup is performed.
  DualDNSResolver(struct ev_loop *loop, int family);

  // Resolves |host|.  |host| must be NULL-terminated string.
  int resolve(const StringRef &host);
  CompleteCb get_complete_cb() const;
  void set_complete_cb(CompleteCb cb);
  DNSResolverStatus get_status(Address *result) const;

private:
  // IP version preference.
  int family_;
  // For A record
  DNSResolver resolv4_;
  // For AAAA record
  DNSResolver resolv6_;
  CompleteCb complete_cb_;
};

} // namespace shrpx

#endif // SHRPX_DUAL_DNS_RESOLVER_H
