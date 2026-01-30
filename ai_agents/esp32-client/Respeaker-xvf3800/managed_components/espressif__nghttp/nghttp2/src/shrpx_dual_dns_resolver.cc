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
#include "shrpx_dual_dns_resolver.h"

namespace shrpx {

DualDNSResolver::DualDNSResolver(struct ev_loop *loop, int family)
  : family_(family), resolv4_(loop), resolv6_(loop) {
  auto cb = [this](DNSResolverStatus, const Address *) {
    Address result;

    auto status = this->get_status(&result);
    switch (status) {
    case DNSResolverStatus::ERROR:
    case DNSResolverStatus::OK:
      break;
    default:
      return;
    }

    auto cb = this->get_complete_cb();
    cb(status, &result);
  };

  if (family_ == AF_UNSPEC || family_ == AF_INET) {
    resolv4_.set_complete_cb(cb);
  }
  if (family_ == AF_UNSPEC || family_ == AF_INET6) {
    resolv6_.set_complete_cb(cb);
  }
}

int DualDNSResolver::resolve(const StringRef &host) {
  int rv4 = 0, rv6 = 0;
  if (family_ == AF_UNSPEC || family_ == AF_INET) {
    rv4 = resolv4_.resolve(host, AF_INET);
  }
  if (family_ == AF_UNSPEC || family_ == AF_INET6) {
    rv6 = resolv6_.resolve(host, AF_INET6);
  }

  if (rv4 != 0 && rv6 != 0) {
    return -1;
  }

  return 0;
}

CompleteCb DualDNSResolver::get_complete_cb() const { return complete_cb_; }

void DualDNSResolver::set_complete_cb(CompleteCb cb) { complete_cb_ = cb; }

DNSResolverStatus DualDNSResolver::get_status(Address *result) const {
  auto rv6 = resolv6_.get_status(result);
  if (rv6 == DNSResolverStatus::OK) {
    return DNSResolverStatus::OK;
  }
  auto rv4 = resolv4_.get_status(result);
  if (rv4 == DNSResolverStatus::OK) {
    return DNSResolverStatus::OK;
  }
  if (rv4 == DNSResolverStatus::RUNNING || rv6 == DNSResolverStatus::RUNNING) {
    return DNSResolverStatus::RUNNING;
  }
  if (rv4 == DNSResolverStatus::ERROR || rv6 == DNSResolverStatus::ERROR) {
    return DNSResolverStatus::ERROR;
  }
  return DNSResolverStatus::IDLE;
}

} // namespace shrpx
