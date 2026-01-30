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
#include "shrpx_downstream_queue.h"

#include <cassert>
#include <limits>

#include "shrpx_downstream.h"

namespace shrpx {

DownstreamQueue::HostEntry::HostEntry(ImmutableString &&key)
  : key(std::move(key)), num_active(0) {}

DownstreamQueue::DownstreamQueue(size_t conn_max_per_host, bool unified_host)
  : conn_max_per_host_(conn_max_per_host == 0
                         ? std::numeric_limits<size_t>::max()
                         : conn_max_per_host),
    unified_host_(unified_host) {}

DownstreamQueue::~DownstreamQueue() {
  dlist_delete_all(downstreams_);
  for (auto &p : host_entries_) {
    auto &ent = p.second;
    dlist_delete_all(ent.blocked);
  }
}

void DownstreamQueue::add_pending(std::unique_ptr<Downstream> downstream) {
  downstream->set_dispatch_state(DispatchState::PENDING);
  downstreams_.append(downstream.release());
}

void DownstreamQueue::mark_failure(Downstream *downstream) {
  downstream->set_dispatch_state(DispatchState::FAILURE);
}

DownstreamQueue::HostEntry &
DownstreamQueue::find_host_entry(const StringRef &host) {
  auto itr = host_entries_.find(host);
  if (itr == std::end(host_entries_)) {
    auto key = ImmutableString{std::begin(host), std::end(host)};
    auto key_ref = StringRef{key};
#ifdef HAVE_STD_MAP_EMPLACE
    std::tie(itr, std::ignore) =
      host_entries_.emplace(key_ref, HostEntry(std::move(key)));
#else  // !HAVE_STD_MAP_EMPLACE
    // for g++-4.7
    std::tie(itr, std::ignore) =
      host_entries_.insert(std::make_pair(key_ref, HostEntry(std::move(key))));
#endif // !HAVE_STD_MAP_EMPLACE
  }
  return (*itr).second;
}

StringRef DownstreamQueue::make_host_key(const StringRef &host) const {
  return unified_host_ ? StringRef{} : host;
}

StringRef DownstreamQueue::make_host_key(Downstream *downstream) const {
  return make_host_key(downstream->request().authority);
}

void DownstreamQueue::mark_active(Downstream *downstream) {
  auto &ent = find_host_entry(make_host_key(downstream));
  ++ent.num_active;

  downstream->set_dispatch_state(DispatchState::ACTIVE);
}

void DownstreamQueue::mark_blocked(Downstream *downstream) {
  auto &ent = find_host_entry(make_host_key(downstream));

  downstream->set_dispatch_state(DispatchState::BLOCKED);

  auto link = new BlockedLink{};
  downstream->attach_blocked_link(link);
  ent.blocked.append(link);
}

bool DownstreamQueue::can_activate(const StringRef &host) const {
  auto itr = host_entries_.find(make_host_key(host));
  if (itr == std::end(host_entries_)) {
    return true;
  }
  auto &ent = (*itr).second;
  return ent.num_active < conn_max_per_host_;
}

namespace {
bool remove_host_entry_if_empty(const DownstreamQueue::HostEntry &ent,
                                DownstreamQueue::HostEntryMap &host_entries,
                                const StringRef &host) {
  if (ent.blocked.empty() && ent.num_active == 0) {
    host_entries.erase(host);
    return true;
  }
  return false;
}
} // namespace

Downstream *DownstreamQueue::remove_and_get_blocked(Downstream *downstream,
                                                    bool next_blocked) {
  // Delete downstream when this function returns.
  auto delptr = std::unique_ptr<Downstream>(downstream);

  downstreams_.remove(downstream);

  auto host = make_host_key(downstream);
  auto &ent = find_host_entry(host);

  if (downstream->get_dispatch_state() == DispatchState::ACTIVE) {
    --ent.num_active;
  } else {
    // For those downstreams deleted while in blocked state
    auto link = downstream->detach_blocked_link();
    if (link) {
      ent.blocked.remove(link);
      delete link;
    }
  }

  if (remove_host_entry_if_empty(ent, host_entries_, host)) {
    return nullptr;
  }

  if (!next_blocked || ent.num_active >= conn_max_per_host_) {
    return nullptr;
  }

  auto link = ent.blocked.head;

  if (!link) {
    return nullptr;
  }

  auto next_downstream = link->downstream;
  auto link2 = next_downstream->detach_blocked_link();
  // This is required with --disable-assert.
  (void)link2;
  assert(link2 == link);
  ent.blocked.remove(link);
  delete link;
  remove_host_entry_if_empty(ent, host_entries_, host);

  return next_downstream;
}

Downstream *DownstreamQueue::get_downstreams() const {
  return downstreams_.head;
}

} // namespace shrpx
