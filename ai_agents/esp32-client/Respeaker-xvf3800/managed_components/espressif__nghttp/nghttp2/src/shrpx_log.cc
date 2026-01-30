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
#include "shrpx_log.h"

#ifdef HAVE_SYSLOG_H
#  include <syslog.h>
#endif // HAVE_SYSLOG_H
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#endif // HAVE_INTTYPES_H
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif // HAVE_FCNTL_H
#include <sys/wait.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iomanip>

#include "shrpx_config.h"
#include "shrpx_downstream.h"
#include "shrpx_worker.h"
#include "util.h"
#include "template.h"

using namespace nghttp2;

namespace shrpx {

namespace {
constexpr StringRef SEVERITY_STR[] = {"INFO"_sr, "NOTICE"_sr, "WARN"_sr,
                                      "ERROR"_sr, "FATAL"_sr};
} // namespace

namespace {
constexpr const char *SEVERITY_COLOR[] = {
  "\033[1;32m", // INFO
  "\033[1;36m", // NOTICE
  "\033[1;33m", // WARN
  "\033[1;31m", // ERROR
  "\033[1;35m", // FATAL
};
} // namespace

#ifndef NOTHREADS
#  ifdef HAVE_THREAD_LOCAL
namespace {
thread_local LogBuffer logbuf_;
} // namespace

namespace {
LogBuffer *get_logbuf() { return &logbuf_; }
} // namespace
#  else  // !HAVE_THREAD_LOCAL
namespace {
pthread_key_t lckey;
pthread_once_t lckey_once = PTHREAD_ONCE_INIT;
} // namespace

namespace {
void make_key() { pthread_key_create(&lckey, nullptr); }
} // namespace

LogBuffer *get_logbuf() {
  pthread_once(&lckey_once, make_key);
  auto buf = static_cast<LogBuffer *>(pthread_getspecific(lckey));
  if (!buf) {
    buf = new LogBuffer();
    pthread_setspecific(lckey, buf);
  }
  return buf;
}
#  endif // !HAVE_THREAD_LOCAL
#else    // NOTHREADS
namespace {
LogBuffer *get_logbuf() {
  static LogBuffer logbuf;
  return &logbuf;
}
} // namespace
#endif   // NOTHREADS

int Log::severity_thres_ = NOTICE;

void Log::set_severity_level(int severity) { severity_thres_ = severity; }

int Log::get_severity_level_by_name(const StringRef &name) {
  for (size_t i = 0, max = array_size(SEVERITY_STR); i < max; ++i) {
    if (name == SEVERITY_STR[i]) {
      return i;
    }
  }
  return -1;
}

int severity_to_syslog_level(int severity) {
  switch (severity) {
  case (INFO):
    return LOG_INFO;
  case (NOTICE):
    return LOG_NOTICE;
  case (WARN):
    return LOG_WARNING;
  case (ERROR):
    return LOG_ERR;
  case (FATAL):
    return LOG_CRIT;
  default:
    return -1;
  }
}

Log::Log(int severity, const char *filename, int linenum)
  : buf_(*get_logbuf()),
    begin_(buf_.data()),
    end_(begin_ + buf_.size()),
    last_(begin_),
    filename_(filename),
    flags_(0),
    severity_(severity),
    linenum_(linenum),
    full_(false) {}

Log::~Log() {
  int rv;
  auto config = get_config();

  if (!config) {
    return;
  }

  auto lgconf = log_config();

  auto &errorconf = config->logging.error;

  if (!log_enabled(severity_) ||
      (lgconf->errorlog_fd == -1 && !errorconf.syslog)) {
    return;
  }

  if (errorconf.syslog) {
    if (severity_ == NOTICE) {
      syslog(severity_to_syslog_level(severity_), "[%s] %.*s",
             SEVERITY_STR[severity_].data(), static_cast<int>(rleft()), begin_);
    } else {
      syslog(severity_to_syslog_level(severity_), "[%s] %.*s (%s:%d)",
             SEVERITY_STR[severity_].data(), static_cast<int>(rleft()), begin_,
             filename_, linenum_);
    }

    return;
  }

  char buf[4_k];
  auto tty = lgconf->errorlog_tty;

  lgconf->update_tstamp_millis(std::chrono::system_clock::now());

  // Error log format: <datetime> <main-pid> <current-pid>
  // <thread-id> <level> (<filename>:<line>) <msg>
  rv = snprintf(buf, sizeof(buf), "%s %d %d %s %s%s%s (%s:%d) %.*s\n",
                lgconf->tstamp->time_iso8601.data(), config->pid, lgconf->pid,
                lgconf->thread_id.c_str(), tty ? SEVERITY_COLOR[severity_] : "",
                SEVERITY_STR[severity_].data(), tty ? "\033[0m" : "", filename_,
                linenum_, static_cast<int>(rleft()), begin_);

  if (rv < 0) {
    return;
  }

  auto nwrite = std::min(static_cast<size_t>(rv), sizeof(buf) - 1);

  while (write(lgconf->errorlog_fd, buf, nwrite) == -1 && errno == EINTR)
    ;
}

Log &Log::operator<<(const std::string &s) {
  write_seq(std::begin(s), std::end(s));
  return *this;
}

Log &Log::operator<<(const StringRef &s) {
  write_seq(std::begin(s), std::end(s));
  return *this;
}

Log &Log::operator<<(const char *s) {
  write_seq(s, s + strlen(s));
  return *this;
}

Log &Log::operator<<(const ImmutableString &s) {
  write_seq(std::begin(s), std::end(s));
  return *this;
}

Log &Log::operator<<(long long n) {
  if (n >= 0) {
    return *this << static_cast<uint64_t>(n);
  }

  if (flags_ & fmt_hex) {
    write_hex(n);
    return *this;
  }

  if (full_) {
    return *this;
  }

  n *= -1;

  size_t nlen = 0;
  for (auto t = n; t; t /= 10, ++nlen)
    ;
  if (wleft() < 1 /* sign */ + nlen) {
    full_ = true;
    return *this;
  }
  *last_++ = '-';
  last_ += nlen;
  update_full();

  auto p = last_ - 1;
  for (; n; n /= 10) {
    *p-- = (n % 10) + '0';
  }
  return *this;
}

Log &Log::operator<<(unsigned long long n) {
  if (flags_ & fmt_hex) {
    write_hex(n);
    return *this;
  }

  if (full_) {
    return *this;
  }

  if (n == 0) {
    *last_++ = '0';
    update_full();
    return *this;
  }
  size_t nlen = 0;
  for (auto t = n; t; t /= 10, ++nlen)
    ;
  if (wleft() < nlen) {
    full_ = true;
    return *this;
  }

  last_ += nlen;
  update_full();

  auto p = last_ - 1;
  for (; n; n /= 10) {
    *p-- = (n % 10) + '0';
  }
  return *this;
}

Log &Log::operator<<(double n) {
  if (full_) {
    return *this;
  }

  auto left = wleft();
  auto rv = snprintf(reinterpret_cast<char *>(last_), left, "%.9f", n);
  if (rv > static_cast<int>(left)) {
    full_ = true;
    return *this;
  }

  last_ += rv;
  update_full();

  return *this;
}

Log &Log::operator<<(long double n) {
  if (full_) {
    return *this;
  }

  auto left = wleft();
  auto rv = snprintf(reinterpret_cast<char *>(last_), left, "%.9Lf", n);
  if (rv > static_cast<int>(left)) {
    full_ = true;
    return *this;
  }

  last_ += rv;
  update_full();

  return *this;
}

Log &Log::operator<<(bool n) {
  if (full_) {
    return *this;
  }

  *last_++ = n ? '1' : '0';
  update_full();

  return *this;
}

Log &Log::operator<<(const void *p) {
  if (full_) {
    return *this;
  }

  write_hex(reinterpret_cast<uintptr_t>(p));

  return *this;
}

namespace log {
void hex(Log &log) { log.set_flags(Log::fmt_hex); }

void dec(Log &log) { log.set_flags(Log::fmt_dec); }
} // namespace log

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator> copy(const char *src, size_t srclen,
                                               OutputIterator d_first,
                                               OutputIterator d_last) {
  auto nwrite =
    std::min(static_cast<size_t>(std::distance(d_first, d_last)), srclen);
  return std::make_pair(std::copy_n(src, nwrite, d_first), d_last);
}
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator>
copy(const char *src, OutputIterator d_first, OutputIterator d_last) {
  return copy(src, strlen(src), d_first, d_last);
}
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator>
copy(const StringRef &src, OutputIterator d_first, OutputIterator d_last) {
  return copy(src.data(), src.size(), d_first, d_last);
}
} // namespace

namespace {
template <size_t N, typename OutputIterator>
std::pair<OutputIterator, OutputIterator>
copy_l(const char (&src)[N], OutputIterator d_first, OutputIterator d_last) {
  return copy(src, N - 1, d_first, d_last);
}
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator> copy(char c, OutputIterator d_first,
                                               OutputIterator d_last) {
  if (d_first == d_last) {
    return std::make_pair(d_last, d_last);
  }
  *d_first++ = c;
  return std::make_pair(d_first, d_last);
}
} // namespace

namespace {
constexpr char LOWER_XDIGITS[] = "0123456789abcdef";
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator>
copy_hex_low(const uint8_t *src, size_t srclen, OutputIterator d_first,
             OutputIterator d_last) {
  auto nwrite =
    std::min(static_cast<size_t>(std::distance(d_first, d_last)), srclen * 2) /
    2;
  for (size_t i = 0; i < nwrite; ++i) {
    *d_first++ = LOWER_XDIGITS[src[i] >> 4];
    *d_first++ = LOWER_XDIGITS[src[i] & 0xf];
  }
  return std::make_pair(d_first, d_last);
}
} // namespace

namespace {
template <typename OutputIterator, typename T>
std::pair<OutputIterator, OutputIterator> copy(T n, OutputIterator d_first,
                                               OutputIterator d_last) {
  if (static_cast<size_t>(std::distance(d_first, d_last)) <
      NGHTTP2_MAX_UINT64_DIGITS) {
    return std::make_pair(d_last, d_last);
  }
  return std::make_pair(util::utos(d_first, n), d_last);
}
} // namespace

namespace {
// 1 means that character must be escaped as "\xNN", where NN is ascii
// code of the character in hex notation.
constexpr uint8_t ESCAPE_TBL[] = {
  1 /* NUL  */, 1 /* SOH  */, 1 /* STX  */, 1 /* ETX  */, 1 /* EOT  */,
  1 /* ENQ  */, 1 /* ACK  */, 1 /* BEL  */, 1 /* BS   */, 1 /* HT   */,
  1 /* LF   */, 1 /* VT   */, 1 /* FF   */, 1 /* CR   */, 1 /* SO   */,
  1 /* SI   */, 1 /* DLE  */, 1 /* DC1  */, 1 /* DC2  */, 1 /* DC3  */,
  1 /* DC4  */, 1 /* NAK  */, 1 /* SYN  */, 1 /* ETB  */, 1 /* CAN  */,
  1 /* EM   */, 1 /* SUB  */, 1 /* ESC  */, 1 /* FS   */, 1 /* GS   */,
  1 /* RS   */, 1 /* US   */, 0 /* SPC  */, 0 /* !    */, 1 /* "    */,
  0 /* #    */, 0 /* $    */, 0 /* %    */, 0 /* &    */, 0 /* '    */,
  0 /* (    */, 0 /* )    */, 0 /* *    */, 0 /* +    */, 0 /* ,    */,
  0 /* -    */, 0 /* .    */, 0 /* /    */, 0 /* 0    */, 0 /* 1    */,
  0 /* 2    */, 0 /* 3    */, 0 /* 4    */, 0 /* 5    */, 0 /* 6    */,
  0 /* 7    */, 0 /* 8    */, 0 /* 9    */, 0 /* :    */, 0 /* ;    */,
  0 /* <    */, 0 /* =    */, 0 /* >    */, 0 /* ?    */, 0 /* @    */,
  0 /* A    */, 0 /* B    */, 0 /* C    */, 0 /* D    */, 0 /* E    */,
  0 /* F    */, 0 /* G    */, 0 /* H    */, 0 /* I    */, 0 /* J    */,
  0 /* K    */, 0 /* L    */, 0 /* M    */, 0 /* N    */, 0 /* O    */,
  0 /* P    */, 0 /* Q    */, 0 /* R    */, 0 /* S    */, 0 /* T    */,
  0 /* U    */, 0 /* V    */, 0 /* W    */, 0 /* X    */, 0 /* Y    */,
  0 /* Z    */, 0 /* [    */, 1 /* \    */, 0 /* ]    */, 0 /* ^    */,
  0 /* _    */, 0 /* `    */, 0 /* a    */, 0 /* b    */, 0 /* c    */,
  0 /* d    */, 0 /* e    */, 0 /* f    */, 0 /* g    */, 0 /* h    */,
  0 /* i    */, 0 /* j    */, 0 /* k    */, 0 /* l    */, 0 /* m    */,
  0 /* n    */, 0 /* o    */, 0 /* p    */, 0 /* q    */, 0 /* r    */,
  0 /* s    */, 0 /* t    */, 0 /* u    */, 0 /* v    */, 0 /* w    */,
  0 /* x    */, 0 /* y    */, 0 /* z    */, 0 /* {    */, 0 /* |    */,
  0 /* }    */, 0 /* ~    */, 1 /* DEL  */, 1 /* 0x80 */, 1 /* 0x81 */,
  1 /* 0x82 */, 1 /* 0x83 */, 1 /* 0x84 */, 1 /* 0x85 */, 1 /* 0x86 */,
  1 /* 0x87 */, 1 /* 0x88 */, 1 /* 0x89 */, 1 /* 0x8a */, 1 /* 0x8b */,
  1 /* 0x8c */, 1 /* 0x8d */, 1 /* 0x8e */, 1 /* 0x8f */, 1 /* 0x90 */,
  1 /* 0x91 */, 1 /* 0x92 */, 1 /* 0x93 */, 1 /* 0x94 */, 1 /* 0x95 */,
  1 /* 0x96 */, 1 /* 0x97 */, 1 /* 0x98 */, 1 /* 0x99 */, 1 /* 0x9a */,
  1 /* 0x9b */, 1 /* 0x9c */, 1 /* 0x9d */, 1 /* 0x9e */, 1 /* 0x9f */,
  1 /* 0xa0 */, 1 /* 0xa1 */, 1 /* 0xa2 */, 1 /* 0xa3 */, 1 /* 0xa4 */,
  1 /* 0xa5 */, 1 /* 0xa6 */, 1 /* 0xa7 */, 1 /* 0xa8 */, 1 /* 0xa9 */,
  1 /* 0xaa */, 1 /* 0xab */, 1 /* 0xac */, 1 /* 0xad */, 1 /* 0xae */,
  1 /* 0xaf */, 1 /* 0xb0 */, 1 /* 0xb1 */, 1 /* 0xb2 */, 1 /* 0xb3 */,
  1 /* 0xb4 */, 1 /* 0xb5 */, 1 /* 0xb6 */, 1 /* 0xb7 */, 1 /* 0xb8 */,
  1 /* 0xb9 */, 1 /* 0xba */, 1 /* 0xbb */, 1 /* 0xbc */, 1 /* 0xbd */,
  1 /* 0xbe */, 1 /* 0xbf */, 1 /* 0xc0 */, 1 /* 0xc1 */, 1 /* 0xc2 */,
  1 /* 0xc3 */, 1 /* 0xc4 */, 1 /* 0xc5 */, 1 /* 0xc6 */, 1 /* 0xc7 */,
  1 /* 0xc8 */, 1 /* 0xc9 */, 1 /* 0xca */, 1 /* 0xcb */, 1 /* 0xcc */,
  1 /* 0xcd */, 1 /* 0xce */, 1 /* 0xcf */, 1 /* 0xd0 */, 1 /* 0xd1 */,
  1 /* 0xd2 */, 1 /* 0xd3 */, 1 /* 0xd4 */, 1 /* 0xd5 */, 1 /* 0xd6 */,
  1 /* 0xd7 */, 1 /* 0xd8 */, 1 /* 0xd9 */, 1 /* 0xda */, 1 /* 0xdb */,
  1 /* 0xdc */, 1 /* 0xdd */, 1 /* 0xde */, 1 /* 0xdf */, 1 /* 0xe0 */,
  1 /* 0xe1 */, 1 /* 0xe2 */, 1 /* 0xe3 */, 1 /* 0xe4 */, 1 /* 0xe5 */,
  1 /* 0xe6 */, 1 /* 0xe7 */, 1 /* 0xe8 */, 1 /* 0xe9 */, 1 /* 0xea */,
  1 /* 0xeb */, 1 /* 0xec */, 1 /* 0xed */, 1 /* 0xee */, 1 /* 0xef */,
  1 /* 0xf0 */, 1 /* 0xf1 */, 1 /* 0xf2 */, 1 /* 0xf3 */, 1 /* 0xf4 */,
  1 /* 0xf5 */, 1 /* 0xf6 */, 1 /* 0xf7 */, 1 /* 0xf8 */, 1 /* 0xf9 */,
  1 /* 0xfa */, 1 /* 0xfb */, 1 /* 0xfc */, 1 /* 0xfd */, 1 /* 0xfe */,
  1 /* 0xff */,
};
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator>
copy_escape(const char *src, size_t srclen, OutputIterator d_first,
            OutputIterator d_last) {
  auto safe_first = src;
  for (auto p = src; p != src + srclen && d_first != d_last; ++p) {
    unsigned char c = *p;
    if (!ESCAPE_TBL[c]) {
      continue;
    }

    auto n =
      std::min(std::distance(d_first, d_last), std::distance(safe_first, p));
    d_first = std::copy_n(safe_first, n, d_first);
    if (std::distance(d_first, d_last) < 4) {
      return std::make_pair(d_first, d_last);
    }
    *d_first++ = '\\';
    *d_first++ = 'x';
    *d_first++ = LOWER_XDIGITS[c >> 4];
    *d_first++ = LOWER_XDIGITS[c & 0xf];
    safe_first = p + 1;
  }

  auto n = std::min(std::distance(d_first, d_last),
                    std::distance(safe_first, src + srclen));
  return std::make_pair(std::copy_n(safe_first, n, d_first), d_last);
}
} // namespace

namespace {
template <typename OutputIterator>
std::pair<OutputIterator, OutputIterator> copy_escape(const StringRef &src,
                                                      OutputIterator d_first,
                                                      OutputIterator d_last) {
  return copy_escape(src.data(), src.size(), d_first, d_last);
}
} // namespace

namespace {
// Construct absolute request URI from |Request|, mainly to log
// request URI for proxy request (HTTP/2 proxy or client proxy).  This
// is mostly same routine found in
// HttpDownstreamConnection::push_request_headers(), but vastly
// simplified since we only care about absolute URI.
StringRef construct_absolute_request_uri(BlockAllocator &balloc,
                                         const Request &req) {
  if (req.authority.empty()) {
    return req.path;
  }

  auto len = req.authority.size() + req.path.size();
  if (req.scheme.empty()) {
    len += str_size("http://");
  } else {
    len += req.scheme.size() + str_size("://");
  }

  auto iov = make_byte_ref(balloc, len + 1);
  auto p = std::begin(iov);

  if (req.scheme.empty()) {
    // We may have to log the request which lacks scheme (e.g.,
    // http/1.1 with origin form).
    p = util::copy_lit(p, "http://");
  } else {
    p = std::copy(std::begin(req.scheme), std::end(req.scheme), p);
    p = util::copy_lit(p, "://");
  }
  p = std::copy(std::begin(req.authority), std::end(req.authority), p);
  p = std::copy(std::begin(req.path), std::end(req.path), p);
  *p = '\0';

  return StringRef{std::span{std::begin(iov), p}};
}
} // namespace

void upstream_accesslog(const std::vector<LogFragment> &lfv,
                        const LogSpec &lgsp) {
  auto config = get_config();
  auto lgconf = log_config();
  auto &accessconf = get_config()->logging.access;

  if (lgconf->accesslog_fd == -1 && !accessconf.syslog) {
    return;
  }

  std::array<char, 4_k> buf;

  auto downstream = lgsp.downstream;

  const auto &req = downstream->request();
  const auto &resp = downstream->response();
  const auto &tstamp = req.tstamp;
  auto &balloc = downstream->get_block_allocator();

  auto downstream_addr = downstream->get_addr();
  auto method =
    req.method == -1 ? "<unknown>"_sr : http2::to_method_string(req.method);
  auto path = req.method == HTTP_CONNECT ? req.authority
              : config->http2_proxy
                ? construct_absolute_request_uri(balloc, req)
              : req.path.empty() ? req.method == HTTP_OPTIONS ? "*"_sr : "-"_sr
                                 : req.path;
  auto path_without_query =
    req.method == HTTP_CONNECT
      ? path
      : StringRef{std::begin(path),
                  std::find(std::begin(path), std::end(path), '?')};

  auto p = std::begin(buf);
  auto last = std::end(buf) - 2;

  for (auto &lf : lfv) {
    switch (lf.type) {
    case LogFragmentType::LITERAL:
      std::tie(p, last) = copy(lf.value, p, last);
      break;
    case LogFragmentType::REMOTE_ADDR:
      std::tie(p, last) = copy(lgsp.remote_addr, p, last);
      break;
    case LogFragmentType::TIME_LOCAL:
      std::tie(p, last) = copy(tstamp->time_local, p, last);
      break;
    case LogFragmentType::TIME_ISO8601:
      std::tie(p, last) = copy(tstamp->time_iso8601, p, last);
      break;
    case LogFragmentType::REQUEST:
      std::tie(p, last) = copy(method, p, last);
      std::tie(p, last) = copy(' ', p, last);
      std::tie(p, last) = copy_escape(path, p, last);
      std::tie(p, last) = copy_l(" HTTP/", p, last);
      std::tie(p, last) = copy(req.http_major, p, last);
      if (req.http_major < 2) {
        std::tie(p, last) = copy('.', p, last);
        std::tie(p, last) = copy(req.http_minor, p, last);
      }
      break;
    case LogFragmentType::METHOD:
      std::tie(p, last) = copy(method, p, last);
      break;
    case LogFragmentType::PATH:
      std::tie(p, last) = copy_escape(path, p, last);
      break;
    case LogFragmentType::PATH_WITHOUT_QUERY:
      std::tie(p, last) = copy_escape(path_without_query, p, last);
      break;
    case LogFragmentType::PROTOCOL_VERSION:
      std::tie(p, last) = copy_l("HTTP/", p, last);
      std::tie(p, last) = copy(req.http_major, p, last);
      if (req.http_major < 2) {
        std::tie(p, last) = copy('.', p, last);
        std::tie(p, last) = copy(req.http_minor, p, last);
      }
      break;
    case LogFragmentType::STATUS:
      std::tie(p, last) = copy(resp.http_status, p, last);
      break;
    case LogFragmentType::BODY_BYTES_SENT:
      std::tie(p, last) = copy(downstream->response_sent_body_length, p, last);
      break;
    case LogFragmentType::HTTP: {
      auto hd = req.fs.header(lf.value);
      if (hd) {
        std::tie(p, last) = copy_escape((*hd).value, p, last);
        break;
      }

      std::tie(p, last) = copy('-', p, last);

      break;
    }
    case LogFragmentType::AUTHORITY:
      if (!req.authority.empty()) {
        std::tie(p, last) = copy(req.authority, p, last);
        break;
      }

      std::tie(p, last) = copy('-', p, last);

      break;
    case LogFragmentType::REMOTE_PORT:
      std::tie(p, last) = copy(lgsp.remote_port, p, last);
      break;
    case LogFragmentType::SERVER_PORT:
      std::tie(p, last) = copy(lgsp.server_port, p, last);
      break;
    case LogFragmentType::REQUEST_TIME: {
      auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
                 lgsp.request_end_time - downstream->get_request_start_time())
                 .count();
      std::tie(p, last) = copy(t / 1000, p, last);
      std::tie(p, last) = copy('.', p, last);
      auto frac = t % 1000;
      if (frac < 100) {
        auto n = frac < 10 ? 2 : 1;
        std::tie(p, last) = copy("000", n, p, last);
      }
      std::tie(p, last) = copy(frac, p, last);
      break;
    }
    case LogFragmentType::PID:
      std::tie(p, last) = copy(lgsp.pid, p, last);
      break;
    case LogFragmentType::ALPN:
      std::tie(p, last) = copy_escape(lgsp.alpn, p, last);
      break;
    case LogFragmentType::TLS_CIPHER:
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy(SSL_get_cipher_name(lgsp.ssl), p, last);
      break;
    case LogFragmentType::TLS_PROTOCOL:
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) =
        copy(nghttp2::tls::get_tls_protocol(lgsp.ssl), p, last);
      break;
    case LogFragmentType::TLS_SESSION_ID: {
      auto session = SSL_get_session(lgsp.ssl);
      if (!session) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      unsigned int session_id_length = 0;
      auto session_id = SSL_SESSION_get_id(session, &session_id_length);
      if (session_id_length == 0) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy_hex_low(session_id, session_id_length, p, last);
      break;
    }
    case LogFragmentType::TLS_SESSION_REUSED:
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) =
        copy(SSL_session_reused(lgsp.ssl) ? 'r' : '.', p, last);
      break;
    case LogFragmentType::TLS_SNI:
      if (lgsp.sni.empty()) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy_escape(lgsp.sni, p, last);
      break;
    case LogFragmentType::TLS_CLIENT_FINGERPRINT_SHA1:
    case LogFragmentType::TLS_CLIENT_FINGERPRINT_SHA256: {
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
#if OPENSSL_3_0_0_API
      auto x = SSL_get0_peer_certificate(lgsp.ssl);
#else  // !OPENSSL_3_0_0_API
      auto x = SSL_get_peer_certificate(lgsp.ssl);
#endif // !OPENSSL_3_0_0_API
      if (!x) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::array<uint8_t, 32> buf;
      auto len = tls::get_x509_fingerprint(
        buf.data(), buf.size(), x,
        lf.type == LogFragmentType::TLS_CLIENT_FINGERPRINT_SHA256 ? EVP_sha256()
                                                                  : EVP_sha1());
#if !OPENSSL_3_0_0_API
      X509_free(x);
#endif // !OPENSSL_3_0_0_API
      if (len <= 0) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy_hex_low(buf.data(), len, p, last);
      break;
    }
    case LogFragmentType::TLS_CLIENT_ISSUER_NAME:
    case LogFragmentType::TLS_CLIENT_SUBJECT_NAME: {
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
#if OPENSSL_3_0_0_API
      auto x = SSL_get0_peer_certificate(lgsp.ssl);
#else  // !OPENSSL_3_0_0_API
      auto x = SSL_get_peer_certificate(lgsp.ssl);
#endif // !OPENSSL_3_0_0_API
      if (!x) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      auto name = lf.type == LogFragmentType::TLS_CLIENT_ISSUER_NAME
                    ? tls::get_x509_issuer_name(balloc, x)
                    : tls::get_x509_subject_name(balloc, x);
#if !OPENSSL_3_0_0_API
      X509_free(x);
#endif // !OPENSSL_3_0_0_API
      if (name.empty()) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy(name, p, last);
      break;
    }
    case LogFragmentType::TLS_CLIENT_SERIAL: {
      if (!lgsp.ssl) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
#if OPENSSL_3_0_0_API
      auto x = SSL_get0_peer_certificate(lgsp.ssl);
#else  // !OPENSSL_3_0_0_API
      auto x = SSL_get_peer_certificate(lgsp.ssl);
#endif // !OPENSSL_3_0_0_API
      if (!x) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      auto sn = tls::get_x509_serial(balloc, x);
#if !OPENSSL_3_0_0_API
      X509_free(x);
#endif // !OPENSSL_3_0_0_API
      if (sn.empty()) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy(sn, p, last);
      break;
    }
    case LogFragmentType::BACKEND_HOST:
      if (!downstream_addr) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy(downstream_addr->host, p, last);
      break;
    case LogFragmentType::BACKEND_PORT:
      if (!downstream_addr) {
        std::tie(p, last) = copy('-', p, last);
        break;
      }
      std::tie(p, last) = copy(downstream_addr->port, p, last);
      break;
    case LogFragmentType::NONE:
      break;
    default:
      break;
    }
  }

  *p = '\0';

  if (accessconf.syslog) {
    syslog(LOG_INFO, "%s", buf.data());

    return;
  }

  *p++ = '\n';

  auto nwrite = std::distance(std::begin(buf), p);
  while (write(lgconf->accesslog_fd, buf.data(), nwrite) == -1 &&
         errno == EINTR)
    ;
}

int reopen_log_files(const LoggingConfig &loggingconf) {
  int res = 0;
  int new_accesslog_fd = -1;
  int new_errorlog_fd = -1;

  auto lgconf = log_config();
  auto &accessconf = loggingconf.access;
  auto &errorconf = loggingconf.error;

  if (!accessconf.syslog && !accessconf.file.empty()) {
    new_accesslog_fd = open_log_file(accessconf.file.data());

    if (new_accesslog_fd == -1) {
      LOG(ERROR) << "Failed to open accesslog file " << accessconf.file;
      res = -1;
    }
  }

  if (!errorconf.syslog && !errorconf.file.empty()) {
    new_errorlog_fd = open_log_file(errorconf.file.data());

    if (new_errorlog_fd == -1) {
      if (lgconf->errorlog_fd != -1) {
        LOG(ERROR) << "Failed to open errorlog file " << errorconf.file;
      } else {
        std::cerr << "Failed to open errorlog file " << errorconf.file
                  << std::endl;
      }

      res = -1;
    }
  }

  close_log_file(lgconf->accesslog_fd);
  close_log_file(lgconf->errorlog_fd);

  lgconf->accesslog_fd = new_accesslog_fd;
  lgconf->errorlog_fd = new_errorlog_fd;
  lgconf->errorlog_tty =
    (new_errorlog_fd == -1) ? false : isatty(new_errorlog_fd);

  return res;
}

void log_chld(pid_t pid, int rstatus, const char *msg) {
  std::string signalstr;
  if (WIFSIGNALED(rstatus)) {
    signalstr += "; signal ";
    auto sig = WTERMSIG(rstatus);
    auto s = strsignal(sig);
    if (s) {
      signalstr += s;
      signalstr += '(';
    } else {
      signalstr += "UNKNOWN(";
    }
    signalstr += util::utos(sig);
    signalstr += ')';
  }

  LOG(NOTICE) << msg << ": [" << pid << "] exited "
              << (WIFEXITED(rstatus) ? "normally" : "abnormally")
              << " with status " << log::hex << rstatus << log::dec
              << "; exit status "
              << (WIFEXITED(rstatus) ? WEXITSTATUS(rstatus) : 0)
              << (signalstr.empty() ? "" : signalstr.c_str());
}

void redirect_stderr_to_errorlog(const LoggingConfig &loggingconf) {
  auto lgconf = log_config();
  auto &errorconf = loggingconf.error;

  if (errorconf.syslog || lgconf->errorlog_fd == -1) {
    return;
  }

  dup2(lgconf->errorlog_fd, STDERR_FILENO);
}

namespace {
int STDERR_COPY = -1;
int STDOUT_COPY = -1;
} // namespace

void store_original_fds() {
  // consider dup'ing stdout too
  STDERR_COPY = dup(STDERR_FILENO);
  STDOUT_COPY = STDOUT_FILENO;
  // no race here, since it is called early
  util::make_socket_closeonexec(STDERR_COPY);
}

void restore_original_fds() { dup2(STDERR_COPY, STDERR_FILENO); }

void close_log_file(int &fd) {
  if (fd != STDERR_COPY && fd != STDOUT_COPY && fd != -1) {
    close(fd);
  }
  fd = -1;
}

int open_log_file(const char *path) {
  if (strcmp(path, "/dev/stdout") == 0 ||
      strcmp(path, "/proc/self/fd/1") == 0) {
    return STDOUT_COPY;
  }

  if (strcmp(path, "/dev/stderr") == 0 ||
      strcmp(path, "/proc/self/fd/2") == 0) {
    return STDERR_COPY;
  }
#ifdef O_CLOEXEC

  auto fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC,
                 S_IRUSR | S_IWUSR | S_IRGRP);
#else // !O_CLOEXEC

  auto fd =
    open(path, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);

  // We get race condition if execve is called at the same time.
  if (fd != -1) {
    util::make_socket_closeonexec(fd);
  }

#endif // !O_CLOEXEC

  if (fd == -1) {
    return -1;
  }

  return fd;
}

} // namespace shrpx
