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
#ifndef SHRPX_ROUTER_H
#define SHRPX_ROUTER_H

#include "shrpx.h"

#include <vector>
#include <memory>

#include "allocator.h"

using namespace nghttp2;

namespace shrpx {

struct RNode {
  RNode();
  RNode(const char *s, size_t len, ssize_t index, ssize_t wildcard_index);
  RNode(RNode &&) = default;
  RNode(const RNode &) = delete;
  RNode &operator=(RNode &&) = default;
  RNode &operator=(const RNode &) = delete;

  // Next RNode, sorted by s[0].
  std::vector<std::unique_ptr<RNode>> next;
  // Stores pointer to the string this node represents.  Not
  // NULL-terminated.
  const char *s;
  // Length of |s|
  size_t len;
  // Index of pattern if match ends in this node.  Note that we don't
  // store duplicated pattern.
  ssize_t index;
  // Index of wildcard pattern if query includes this node as prefix
  // and it still has suffix to match.  Note that we don't store
  // duplicated pattern.
  ssize_t wildcard_index;
};

class Router {
public:
  Router();
  ~Router();
  Router(Router &&) = default;
  Router(const Router &) = delete;
  Router &operator=(Router &&) = default;
  Router &operator=(const Router &) = delete;

  // Adds route |pattern| with its |index|.  If same pattern has
  // already been added, the existing index is returned.  If
  // |wildcard| is true, |pattern| is considered as wildcard pattern,
  // and all paths which have the |pattern| as prefix and are strictly
  // longer than |pattern| match.  The wildcard pattern only works
  // with match(const StringRef&, const StringRef&).
  size_t add_route(const StringRef &pattern, size_t index,
                   bool wildcard = false);
  // Returns the matched index of pattern.  -1 if there is no match.
  ssize_t match(const StringRef &host, const StringRef &path) const;
  // Returns the matched index of pattern |s|.  -1 if there is no
  // match.
  ssize_t match(const StringRef &s) const;
  // Returns the matched index of pattern if a pattern is a suffix of
  // |s|, otherwise -1.  If |*last_node| is not nullptr, it specifies
  // the first node to start matching.  If it is nullptr, match will
  // start from scratch.  When the match was found (the return value
  // is not -1), |*nread| has the number of bytes matched in |s|, and
  // |*last_node| has the last matched node.  One can continue to
  // match the longer pattern using the returned |*last_node| to the
  // another invocation of this function until it returns -1.
  ssize_t match_prefix(size_t *nread, const RNode **last_node,
                       const StringRef &s) const;

  void add_node(RNode *node, const char *pattern, size_t patlen, ssize_t index,
                ssize_t wildcard_index);

  void dump() const;

private:
  BlockAllocator balloc_;
  // The root node of Patricia tree.  This is special node and its s
  // field is nulptr, and len field is 0.
  RNode root_;
};

} // namespace shrpx

#endif // SHRPX_ROUTER_H
