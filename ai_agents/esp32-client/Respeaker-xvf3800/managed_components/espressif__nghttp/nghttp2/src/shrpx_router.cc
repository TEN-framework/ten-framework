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
#include "shrpx_router.h"

#include <algorithm>

#include "shrpx_config.h"
#include "shrpx_log.h"

namespace shrpx {

RNode::RNode() : s(nullptr), len(0), index(-1), wildcard_index(-1) {}

RNode::RNode(const char *s, size_t len, ssize_t index, ssize_t wildcard_index)
  : s(s), len(len), index(index), wildcard_index(wildcard_index) {}

Router::Router() : balloc_(1024, 1024), root_{} {}

Router::~Router() {}

namespace {
RNode *find_next_node(const RNode *node, char c) {
  auto itr = std::lower_bound(std::begin(node->next), std::end(node->next), c,
                              [](const std::unique_ptr<RNode> &lhs,
                                 const char c) { return lhs->s[0] < c; });
  if (itr == std::end(node->next) || (*itr)->s[0] != c) {
    return nullptr;
  }

  return (*itr).get();
}
} // namespace

namespace {
void add_next_node(RNode *node, std::unique_ptr<RNode> new_node) {
  auto itr = std::lower_bound(std::begin(node->next), std::end(node->next),
                              new_node->s[0],
                              [](const std::unique_ptr<RNode> &lhs,
                                 const char c) { return lhs->s[0] < c; });
  node->next.insert(itr, std::move(new_node));
}
} // namespace

void Router::add_node(RNode *node, const char *pattern, size_t patlen,
                      ssize_t index, ssize_t wildcard_index) {
  auto pat = make_string_ref(balloc_, StringRef{pattern, patlen});
  auto new_node =
    std::make_unique<RNode>(pat.data(), pat.size(), index, wildcard_index);
  add_next_node(node, std::move(new_node));
}

size_t Router::add_route(const StringRef &pattern, size_t idx, bool wildcard) {
  ssize_t index = -1, wildcard_index = -1;
  if (wildcard) {
    wildcard_index = idx;
  } else {
    index = idx;
  }

  auto node = &root_;
  size_t i = 0;

  for (;;) {
    auto next_node = find_next_node(node, pattern[i]);
    if (next_node == nullptr) {
      add_node(node, pattern.data() + i, pattern.size() - i, index,
               wildcard_index);
      return idx;
    }

    node = next_node;

    auto slen = pattern.size() - i;
    auto s = pattern.data() + i;
    auto n = std::min(node->len, slen);
    size_t j;
    for (j = 0; j < n && node->s[j] == s[j]; ++j)
      ;
    if (j == n) {
      // The common prefix was matched
      if (slen == node->len) {
        // Complete match
        if (index != -1) {
          if (node->index != -1) {
            // Return the existing index for duplicates.
            return node->index;
          }
          node->index = index;
          return idx;
        }

        assert(wildcard_index != -1);

        if (node->wildcard_index != -1) {
          return node->wildcard_index;
        }
        node->wildcard_index = wildcard_index;
        return idx;
      }

      if (slen > node->len) {
        // We still have pattern to add
        i += j;

        continue;
      }
    }

    if (node->len > j) {
      // node must be split into 2 nodes.  new_node is now the child
      // of node.
      auto new_node = std::make_unique<RNode>(
        &node->s[j], node->len - j, node->index, node->wildcard_index);
      std::swap(node->next, new_node->next);

      node->len = j;
      node->index = -1;
      node->wildcard_index = -1;

      add_next_node(node, std::move(new_node));

      if (slen == j) {
        node->index = index;
        node->wildcard_index = wildcard_index;
        return idx;
      }
    }

    i += j;

    assert(pattern.size() > i);
    add_node(node, pattern.data() + i, pattern.size() - i, index,
             wildcard_index);

    return idx;
  }
}

namespace {
const RNode *match_complete(size_t *offset, const RNode *node,
                            const char *first, const char *last) {
  *offset = 0;

  if (first == last) {
    return node;
  }

  auto p = first;

  for (;;) {
    auto next_node = find_next_node(node, *p);
    if (next_node == nullptr) {
      return nullptr;
    }

    node = next_node;

    auto n = std::min(node->len, static_cast<size_t>(last - p));
    if (memcmp(node->s, p, n) != 0) {
      return nullptr;
    }
    p += n;
    if (p == last) {
      *offset = n;
      return node;
    }
  }
}
} // namespace

namespace {
const RNode *match_partial(bool *pattern_is_wildcard, const RNode *node,
                           size_t offset, const char *first, const char *last) {
  *pattern_is_wildcard = false;

  if (first == last) {
    if (node->len == offset) {
      return node;
    }
    return nullptr;
  }

  auto p = first;

  const RNode *found_node = nullptr;

  if (offset > 0) {
    auto n = std::min(node->len - offset, static_cast<size_t>(last - first));
    if (memcmp(node->s + offset, first, n) != 0) {
      return nullptr;
    }

    p += n;

    if (p == last) {
      if (node->len == offset + n) {
        if (node->index != -1) {
          return node;
        }

        // The last '/' handling, see below.
        node = find_next_node(node, '/');
        if (node != nullptr && node->index != -1 && node->len == 1) {
          return node;
        }

        return nullptr;
      }

      // The last '/' handling, see below.
      if (node->index != -1 && offset + n + 1 == node->len &&
          node->s[node->len - 1] == '/') {
        return node;
      }

      return nullptr;
    }

    if (node->wildcard_index != -1) {
      found_node = node;
      *pattern_is_wildcard = true;
    } else if (node->index != -1 && node->s[node->len - 1] == '/') {
      found_node = node;
      *pattern_is_wildcard = false;
    }

    assert(node->len == offset + n);
  }

  for (;;) {
    auto next_node = find_next_node(node, *p);
    if (next_node == nullptr) {
      return found_node;
    }

    node = next_node;

    auto n = std::min(node->len, static_cast<size_t>(last - p));
    if (memcmp(node->s, p, n) != 0) {
      return found_node;
    }

    p += n;

    if (p == last) {
      if (node->len == n) {
        // Complete match with this node
        if (node->index != -1) {
          *pattern_is_wildcard = false;
          return node;
        }

        // The last '/' handling, see below.
        node = find_next_node(node, '/');
        if (node != nullptr && node->index != -1 && node->len == 1) {
          *pattern_is_wildcard = false;
          return node;
        }

        return found_node;
      }

      // We allow match without trailing "/" at the end of pattern.
      // So, if pattern ends with '/', and pattern and path matches
      // without that slash, we consider they match to deal with
      // request to the directory without trailing slash.  That is if
      // pattern is "/foo/" and path is "/foo", we consider they
      // match.
      if (node->index != -1 && n + 1 == node->len && node->s[n] == '/') {
        *pattern_is_wildcard = false;
        return node;
      }

      return found_node;
    }

    if (node->wildcard_index != -1) {
      found_node = node;
      *pattern_is_wildcard = true;
    } else if (node->index != -1 && node->s[node->len - 1] == '/') {
      // This is the case when pattern which ends with "/" is included
      // in query.
      found_node = node;
      *pattern_is_wildcard = false;
    }

    assert(node->len == n);
  }
}
} // namespace

ssize_t Router::match(const StringRef &host, const StringRef &path) const {
  const RNode *node;
  size_t offset;

  node = match_complete(&offset, &root_, std::begin(host), std::end(host));
  if (node == nullptr) {
    return -1;
  }

  bool pattern_is_wildcard;
  node = match_partial(&pattern_is_wildcard, node, offset, std::begin(path),
                       std::end(path));
  if (node == nullptr || node == &root_) {
    return -1;
  }

  return pattern_is_wildcard ? node->wildcard_index : node->index;
}

ssize_t Router::match(const StringRef &s) const {
  const RNode *node;
  size_t offset;

  node = match_complete(&offset, &root_, std::begin(s), std::end(s));
  if (node == nullptr) {
    return -1;
  }

  if (node->len != offset) {
    return -1;
  }

  return node->index;
}

namespace {
const RNode *match_prefix(size_t *nread, const RNode *node, const char *first,
                          const char *last) {
  if (first == last) {
    return nullptr;
  }

  auto p = first;

  for (;;) {
    auto next_node = find_next_node(node, *p);
    if (next_node == nullptr) {
      return nullptr;
    }

    node = next_node;

    auto n = std::min(node->len, static_cast<size_t>(last - p));
    if (memcmp(node->s, p, n) != 0) {
      return nullptr;
    }

    p += n;

    if (p != last) {
      if (node->index != -1) {
        *nread = p - first;
        return node;
      }
      continue;
    }

    if (node->len == n) {
      *nread = p - first;
      return node;
    }

    return nullptr;
  }
}
} // namespace

ssize_t Router::match_prefix(size_t *nread, const RNode **last_node,
                             const StringRef &s) const {
  if (*last_node == nullptr) {
    *last_node = &root_;
  }

  auto node =
    ::shrpx::match_prefix(nread, *last_node, std::begin(s), std::end(s));
  if (node == nullptr) {
    return -1;
  }

  *last_node = node;

  return node->index;
}

namespace {
void dump_node(const RNode *node, int depth) {
  fprintf(stderr, "%*ss='%.*s', len=%zu, index=%zd\n", depth, "",
          (int)node->len, node->s, node->len, node->index);
  for (auto &nd : node->next) {
    dump_node(nd.get(), depth + 4);
  }
}
} // namespace

void Router::dump() const { dump_node(&root_, 0); }

} // namespace shrpx
