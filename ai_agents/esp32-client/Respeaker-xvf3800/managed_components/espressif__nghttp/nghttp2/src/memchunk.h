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
#ifndef MEMCHUNK_H
#define MEMCHUNK_H

#include "nghttp2_config.h"

#include <limits.h>
#ifdef _WIN32
/* Structure for scatter/gather I/O.  */
struct iovec {
  void *iov_base; /* Pointer to data.  */
  size_t iov_len; /* Length of data.  */
};
#else // !_WIN32
#  include <sys/uio.h>
#endif // !_WIN32

#include <cassert>
#include <cstring>
#include <memory>
#include <array>
#include <algorithm>
#include <string>
#include <utility>

#include "template.h"

namespace nghttp2 {

#define DEFAULT_WR_IOVCNT 16

#if defined(IOV_MAX) && IOV_MAX < DEFAULT_WR_IOVCNT
#  define MAX_WR_IOVCNT IOV_MAX
#else // !defined(IOV_MAX) || IOV_MAX >= DEFAULT_WR_IOVCNT
#  define MAX_WR_IOVCNT DEFAULT_WR_IOVCNT
#endif // !defined(IOV_MAX) || IOV_MAX >= DEFAULT_WR_IOVCNT

template <size_t N> struct Memchunk {
  Memchunk(Memchunk *next_chunk)
    : pos(std::begin(buf)), last(pos), knext(next_chunk), next(nullptr) {}
  size_t len() const { return last - pos; }
  size_t left() const { return std::end(buf) - last; }
  void reset() { pos = last = std::begin(buf); }
  std::array<uint8_t, N> buf;
  uint8_t *pos, *last;
  Memchunk *knext;
  Memchunk *next;
  static const size_t size = N;
};

template <typename T> struct Pool {
  Pool() : pool(nullptr), freelist(nullptr), poolsize(0), freelistsize(0) {}
  ~Pool() { clear(); }
  T *get() {
    if (freelist) {
      auto m = freelist;
      freelist = freelist->next;
      m->next = nullptr;
      m->reset();
      freelistsize -= T::size;
      return m;
    }

    pool = new T{pool};
    poolsize += T::size;
    return pool;
  }
  void recycle(T *m) {
    m->next = freelist;
    freelist = m;
    freelistsize += T::size;
  }
  void clear() {
    freelist = nullptr;
    freelistsize = 0;
    for (auto p = pool; p;) {
      auto knext = p->knext;
      delete p;
      p = knext;
    }
    pool = nullptr;
    poolsize = 0;
  }
  using value_type = T;
  T *pool;
  T *freelist;
  size_t poolsize;
  size_t freelistsize;
};

template <typename Memchunk> struct Memchunks {
  Memchunks(Pool<Memchunk> *pool)
    : pool(pool),
      head(nullptr),
      tail(nullptr),
      len(0),
      mark(nullptr),
      mark_pos(nullptr),
      mark_offset(0) {}
  Memchunks(const Memchunks &) = delete;
  Memchunks(Memchunks &&other) noexcept
    : pool{other.pool}, // keep other.pool
      head{std::exchange(other.head, nullptr)},
      tail{std::exchange(other.tail, nullptr)},
      len{std::exchange(other.len, 0)},
      mark{std::exchange(other.mark, nullptr)},
      mark_pos{std::exchange(other.mark_pos, nullptr)},
      mark_offset{std::exchange(other.mark_offset, 0)} {}
  Memchunks &operator=(const Memchunks &) = delete;
  Memchunks &operator=(Memchunks &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();

    pool = other.pool;
    head = std::exchange(other.head, nullptr);
    tail = std::exchange(other.tail, nullptr);
    len = std::exchange(other.len, 0);
    mark = std::exchange(other.mark, nullptr);
    mark_pos = std::exchange(other.mark_pos, nullptr);
    mark_offset = std::exchange(other.mark_offset, 0);

    return *this;
  }
  ~Memchunks() {
    if (!pool) {
      return;
    }
    for (auto m = head; m;) {
      auto next = m->next;
      pool->recycle(m);
      m = next;
    }
  }
  size_t append(char c) {
    if (!tail) {
      head = tail = pool->get();
    } else if (tail->left() == 0) {
      tail->next = pool->get();
      tail = tail->next;
    }
    *tail->last++ = c;
    ++len;
    return 1;
  }
  size_t append(const void *src, size_t count) {
    if (count == 0) {
      return 0;
    }

    auto first = static_cast<const uint8_t *>(src);
    auto last = first + count;

    if (!tail) {
      head = tail = pool->get();
    }

    for (;;) {
      auto n = std::min(static_cast<size_t>(last - first), tail->left());
      tail->last = std::copy_n(first, n, tail->last);
      first += n;
      len += n;
      if (first == last) {
        break;
      }

      tail->next = pool->get();
      tail = tail->next;
    }

    return count;
  }
  template <size_t N> size_t append(const char (&s)[N]) {
    return append(s, N - 1);
  }
  size_t append(const std::string &s) { return append(s.c_str(), s.size()); }
  size_t append(const StringRef &s) { return append(s.data(), s.size()); }
  size_t append(const ImmutableString &s) {
    return append(s.c_str(), s.size());
  }
  size_t copy(Memchunks &dest) {
    auto m = head;
    while (m) {
      dest.append(m->pos, m->len());
      m = m->next;
    }
    return len;
  }
  size_t remove(void *dest, size_t count) {
    assert(mark == nullptr);

    if (!tail || count == 0) {
      return 0;
    }

    auto first = static_cast<uint8_t *>(dest);
    auto last = first + count;

    auto m = head;

    while (m) {
      auto next = m->next;
      auto n = std::min(static_cast<size_t>(last - first), m->len());

      assert(m->len());
      first = std::copy_n(m->pos, n, first);
      m->pos += n;
      len -= n;
      if (m->len() > 0) {
        break;
      }
      pool->recycle(m);
      m = next;
    }
    head = m;
    if (head == nullptr) {
      tail = nullptr;
    }

    return first - static_cast<uint8_t *>(dest);
  }
  size_t remove(Memchunks &dest, size_t count) {
    assert(mark == nullptr);

    if (!tail || count == 0) {
      return 0;
    }

    auto left = count;
    auto m = head;

    while (m) {
      auto next = m->next;
      auto n = std::min(left, m->len());

      assert(m->len());
      dest.append(m->pos, n);
      m->pos += n;
      len -= n;
      left -= n;
      if (m->len() > 0) {
        break;
      }
      pool->recycle(m);
      m = next;
    }
    head = m;
    if (head == nullptr) {
      tail = nullptr;
    }

    return count - left;
  }
  size_t remove(Memchunks &dest) {
    assert(pool == dest.pool);
    assert(mark == nullptr);

    if (head == nullptr) {
      return 0;
    }

    auto n = len;

    if (dest.tail == nullptr) {
      dest.head = head;
    } else {
      dest.tail->next = head;
    }

    dest.tail = tail;
    dest.len += len;

    head = tail = nullptr;
    len = 0;

    return n;
  }
  size_t drain(size_t count) {
    assert(mark == nullptr);

    auto ndata = count;
    auto m = head;
    while (m) {
      auto next = m->next;
      auto n = std::min(count, m->len());
      m->pos += n;
      count -= n;
      len -= n;
      if (m->len() > 0) {
        break;
      }

      pool->recycle(m);
      m = next;
    }
    head = m;
    if (head == nullptr) {
      tail = nullptr;
    }
    return ndata - count;
  }
  size_t drain_mark(size_t count) {
    auto ndata = count;
    auto m = head;
    while (m) {
      auto next = m->next;
      auto n = std::min(count, m->len());
      m->pos += n;
      count -= n;
      len -= n;
      mark_offset -= n;

      if (m->len() > 0) {
        assert(mark != m || m->pos <= mark_pos);
        break;
      }
      if (mark == m) {
        assert(m->pos <= mark_pos);

        mark = nullptr;
        mark_pos = nullptr;
        mark_offset = 0;
      }

      pool->recycle(m);
      m = next;
    }
    head = m;
    if (head == nullptr) {
      tail = nullptr;
    }
    return ndata - count;
  }
  int riovec(struct iovec *iov, int iovcnt) const {
    if (!head) {
      return 0;
    }
    auto m = head;
    int i;
    for (i = 0; i < iovcnt && m; ++i, m = m->next) {
      iov[i].iov_base = m->pos;
      iov[i].iov_len = m->len();
    }
    return i;
  }
  int riovec_mark(struct iovec *iov, int iovcnt) {
    if (!head || iovcnt == 0) {
      return 0;
    }

    int i = 0;
    Memchunk *m;
    if (mark) {
      if (mark_pos != mark->last) {
        iov[0].iov_base = mark_pos;
        iov[0].iov_len = mark->len() - (mark_pos - mark->pos);

        mark_pos = mark->last;
        mark_offset += iov[0].iov_len;
        i = 1;
      }
      m = mark->next;
    } else {
      i = 0;
      m = head;
    }

    for (; i < iovcnt && m; ++i, m = m->next) {
      iov[i].iov_base = m->pos;
      iov[i].iov_len = m->len();

      mark = m;
      mark_pos = m->last;
      mark_offset += m->len();
    }

    return i;
  }
  size_t rleft() const { return len; }
  size_t rleft_mark() const { return len - mark_offset; }
  void reset() {
    for (auto m = head; m;) {
      auto next = m->next;
      pool->recycle(m);
      m = next;
    }
    len = 0;
    head = tail = mark = nullptr;
    mark_pos = nullptr;
    mark_offset = 0;
  }

  Pool<Memchunk> *pool;
  Memchunk *head, *tail;
  size_t len;
  Memchunk *mark;
  uint8_t *mark_pos;
  size_t mark_offset;
};

// Wrapper around Memchunks to offer "peeking" functionality.
template <typename Memchunk> struct PeekMemchunks {
  PeekMemchunks(Pool<Memchunk> *pool)
    : memchunks(pool),
      cur(nullptr),
      cur_pos(nullptr),
      cur_last(nullptr),
      len(0),
      peeking(true) {}
  PeekMemchunks(const PeekMemchunks &) = delete;
  PeekMemchunks(PeekMemchunks &&other) noexcept
    : memchunks{std::move(other.memchunks)},
      cur{std::exchange(other.cur, nullptr)},
      cur_pos{std::exchange(other.cur_pos, nullptr)},
      cur_last{std::exchange(other.cur_last, nullptr)},
      len{std::exchange(other.len, 0)},
      peeking{std::exchange(other.peeking, true)} {}
  PeekMemchunks &operator=(const PeekMemchunks &) = delete;
  PeekMemchunks &operator=(PeekMemchunks &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    memchunks = std::move(other.memchunks);
    cur = std::exchange(other.cur, nullptr);
    cur_pos = std::exchange(other.cur_pos, nullptr);
    cur_last = std::exchange(other.cur_last, nullptr);
    len = std::exchange(other.len, 0);
    peeking = std::exchange(other.peeking, true);

    return *this;
  }
  size_t append(const void *src, size_t count) {
    count = memchunks.append(src, count);
    len += count;
    return count;
  }
  size_t remove(void *dest, size_t count) {
    if (!peeking) {
      count = memchunks.remove(dest, count);
      len -= count;
      return count;
    }

    if (count == 0 || len == 0) {
      return 0;
    }

    if (!cur) {
      cur = memchunks.head;
      cur_pos = cur->pos;
    }

    // cur_last could be updated in append
    cur_last = cur->last;

    if (cur_pos == cur_last) {
      assert(cur->next);
      cur = cur->next;
    }

    auto first = static_cast<uint8_t *>(dest);
    auto last = first + count;

    for (;;) {
      auto n = std::min(last - first, cur_last - cur_pos);

      first = std::copy_n(cur_pos, n, first);
      cur_pos += n;
      len -= n;

      if (first == last) {
        break;
      }
      assert(cur_pos == cur_last);
      if (!cur->next) {
        break;
      }
      cur = cur->next;
      cur_pos = cur->pos;
      cur_last = cur->last;
    }
    return first - static_cast<uint8_t *>(dest);
  }
  size_t rleft() const { return len; }
  size_t rleft_buffered() const { return memchunks.rleft(); }
  void disable_peek(bool drain) {
    if (!peeking) {
      return;
    }
    if (drain) {
      auto n = rleft_buffered() - rleft();
      memchunks.drain(n);
      assert(len == memchunks.rleft());
    } else {
      len = memchunks.rleft();
    }
    cur = nullptr;
    cur_pos = cur_last = nullptr;
    peeking = false;
  }
  void reset() {
    memchunks.reset();
    cur = nullptr;
    cur_pos = cur_last = nullptr;
    len = 0;
    peeking = true;
  }
  Memchunks<Memchunk> memchunks;
  // Pointer to the Memchunk currently we are reading/writing.
  Memchunk *cur;
  // Region inside cur, we have processed to cur_pos.
  uint8_t *cur_pos, *cur_last;
  // This is the length we have left unprocessed.  len <=
  // memchunk.rleft() must hold.
  size_t len;
  // true if peeking is enabled.  Initially it is true.
  bool peeking;
};

using Memchunk16K = Memchunk<16_k>;
using MemchunkPool = Pool<Memchunk16K>;
using DefaultMemchunks = Memchunks<Memchunk16K>;
using DefaultPeekMemchunks = PeekMemchunks<Memchunk16K>;

inline int limit_iovec(struct iovec *iov, int iovcnt, size_t max) {
  if (max == 0) {
    return 0;
  }
  for (int i = 0; i < iovcnt; ++i) {
    auto d = std::min(max, iov[i].iov_len);
    iov[i].iov_len = d;
    max -= d;
    if (max == 0) {
      return i + 1;
    }
  }
  return iovcnt;
}

// MemchunkBuffer is similar to Buffer, but it uses pooled Memchunk
// for its underlying buffer.
template <typename Memchunk> struct MemchunkBuffer {
  MemchunkBuffer(Pool<Memchunk> *pool) : pool(pool), chunk(nullptr) {}
  MemchunkBuffer(const MemchunkBuffer &) = delete;
  MemchunkBuffer(MemchunkBuffer &&other) noexcept
    : pool(other.pool), chunk(other.chunk) {
    other.chunk = nullptr;
  }
  MemchunkBuffer &operator=(const MemchunkBuffer &) = delete;
  MemchunkBuffer &operator=(MemchunkBuffer &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    pool = other.pool;
    chunk = other.chunk;

    other.chunk = nullptr;

    return *this;
  }

  ~MemchunkBuffer() {
    if (!pool || !chunk) {
      return;
    }
    pool->recycle(chunk);
  }

  // Ensures that the underlying buffer is allocated.
  void ensure_chunk() {
    if (chunk) {
      return;
    }
    chunk = pool->get();
  }

  // Releases the underlying buffer.
  void release_chunk() {
    if (!chunk) {
      return;
    }
    pool->recycle(chunk);
    chunk = nullptr;
  }

  // Returns true if the underlying buffer is allocated.
  bool chunk_avail() const { return chunk != nullptr; }

  // The functions below must be called after the underlying buffer is
  // allocated (use ensure_chunk).

  // MemchunkBuffer provides the same interface functions with Buffer.
  // Since we has chunk as a member variable, pos and last are
  // implemented as wrapper functions.

  uint8_t *pos() const { return chunk->pos; }
  uint8_t *last() const { return chunk->last; }

  size_t rleft() const { return chunk->len(); }
  size_t wleft() const { return chunk->left(); }
  size_t write(const void *src, size_t count) {
    count = std::min(count, wleft());
    auto p = static_cast<const uint8_t *>(src);
    chunk->last = std::copy_n(p, count, chunk->last);
    return count;
  }
  size_t write(size_t count) {
    count = std::min(count, wleft());
    chunk->last += count;
    return count;
  }
  size_t drain(size_t count) {
    count = std::min(count, rleft());
    chunk->pos += count;
    return count;
  }
  size_t drain_reset(size_t count) {
    count = std::min(count, rleft());
    std::copy(chunk->pos + count, chunk->last, std::begin(chunk->buf));
    chunk->last = std::begin(chunk->buf) + (chunk->last - (chunk->pos + count));
    chunk->pos = std::begin(chunk->buf);
    return count;
  }
  void reset() { chunk->reset(); }
  uint8_t *begin() { return std::begin(chunk->buf); }
  uint8_t &operator[](size_t n) { return chunk->buf[n]; }
  const uint8_t &operator[](size_t n) const { return chunk->buf[n]; }

  Pool<Memchunk> *pool;
  Memchunk *chunk;
};

using DefaultMemchunkBuffer = MemchunkBuffer<Memchunk16K>;

} // namespace nghttp2

#endif // MEMCHUNK_H
