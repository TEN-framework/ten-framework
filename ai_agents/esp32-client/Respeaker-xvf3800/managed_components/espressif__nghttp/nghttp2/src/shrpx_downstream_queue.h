/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#ifndef SHRPX_DOWNSTREAM_QUEUE_H
#define SHRPX_DOWNSTREAM_QUEUE_H

#include "shrpx.h"

#include <cinttypes>
#include <map>
#include <set>
#include <memory>

#include "template.h"

using namespace nghttp2;

namespace shrpx {

class Downstream;

// Link entry in HostEntry.blocked and downstream because downstream
// could be deleted in anytime and we'd like to find Downstream in
// O(1).  Downstream has field to link back to this object.
struct BlockedLink {
  Downstream *downstream;
  BlockedLink *dlnext, *dlprev;
};

class DownstreamQueue {
public:
  struct HostEntry {
    HostEntry(ImmutableString &&key);

    HostEntry(HostEntry &&) = default;
    HostEntry &operator=(HostEntry &&) = default;

    HostEntry(const HostEntry &) = delete;
    HostEntry &operator=(const HostEntry &) = delete;

    // Key that associates this object
    ImmutableString key;
    // Set of stream ID that blocked by conn_max_per_host_.
    DList<BlockedLink> blocked;
    // The number of connections currently made to this host.
    size_t num_active;
  };

  using HostEntryMap = std::map<StringRef, HostEntry>;

  // conn_max_per_host == 0 means no limit for downstream connection.
  DownstreamQueue(size_t conn_max_per_host = 0, bool unified_host = true);
  ~DownstreamQueue();
  // Add |downstream| to this queue.  This is entry point for
  // Downstream object.
  void add_pending(std::unique_ptr<Downstream> downstream);
  // Set |downstream| to failure state, which means that downstream
  // failed to connect to backend.
  void mark_failure(Downstream *downstream);
  // Set |downstream| to active state, which means that downstream
  // connection has started.
  void mark_active(Downstream *downstream);
  // Set |downstream| to blocked state, which means that download
  // connection was blocked because conn_max_per_host_ limit.
  void mark_blocked(Downstream *downstream);
  // Returns true if we can make downstream connection to given
  // |host|.
  bool can_activate(const StringRef &host) const;
  // Removes and frees |downstream| object.  If |downstream| is in
  // DispatchState::ACTIVE, and |next_blocked| is true, this function
  // may return Downstream object with the same target host in
  // DispatchState::BLOCKED if its connection is now not blocked by
  // conn_max_per_host_ limit.
  Downstream *remove_and_get_blocked(Downstream *downstream,
                                     bool next_blocked = true);
  Downstream *get_downstreams() const;
  HostEntry &find_host_entry(const StringRef &host);
  StringRef make_host_key(const StringRef &host) const;
  StringRef make_host_key(Downstream *downstream) const;

private:
  // Per target host structure to keep track of the number of
  // connections to the same host.
  HostEntryMap host_entries_;
  DList<Downstream> downstreams_;
  // Maximum number of concurrent connections to the same host.
  size_t conn_max_per_host_;
  // true if downstream host is treated as the same.  Used for reverse
  // proxying.
  bool unified_host_;
};

} // namespace shrpx

#endif // SHRPX_DOWNSTREAM_QUEUE_H
